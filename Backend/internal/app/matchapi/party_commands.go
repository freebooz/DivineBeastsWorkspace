// 本文件是普通组队命令应用层：认证主体由Gateway传入，领域规则在仓储条件事务中执行。
// 不计算伤害／竞技胜负，不发送HTTP；返回对象只能由传输层按调用者权限投影，不能直接公开全部请求。
package matchapi

import (
	"context"
	"errors"
	"strings"
	"time"

	"divinebeasts/backend/internal/modules/party"
)

// ErrPartyNotFound（不存在队伍）供适配器用errors.Is区分无队伍与存储故障，不能吞掉连接错误。
var ErrPartyNotFound = errors.New("PARTY_NOT_FOUND: 队伍不存在")

// PartyCommand（普通队伍命令）不包含可伪造的actor；ExpectedRevision必须为读取到的非负版本。
type PartyCommand = party.Command

// PartyOperationReceipt（事务回执）只在后端保存；相同主体同键异参冲突，重试返回原提交快照。
type PartyOperationReceipt struct {
	Fingerprint string       // Fingerprint（主体作用域内命令内容摘要），同幂等键异内容必须拒绝。
	Result      *party.Party // Result（已提交聚合快照），只在后端保存，传输前按调用者授权投影。
}

// AtomicPartyRepository（跨进程条件事务端口）同时维护成员唯一索引、请求和聚合修订。
// mutation必须纯内存、无外部副作用，冲突时可被重试；operationKey为空只用于受信内部条件更新。
type AtomicPartyRepository interface {
	PartyRepository
	GetByPlayerID(ctx context.Context, playerID string) (*party.Party, error)
	Update(ctx context.Context, partyID string, expectedRevision int64, operationKey, fingerprint string, mutation func(*party.Party) error) (*party.Party, error)
	GetInvitations(ctx context.Context, playerID string, now time.Time) ([]PartyInvitationView, error)
}

// PartyInvitationView（本人邀请收件项）只含授权展示字段，不暴露其他申请者或完整队伍快照。
type PartyInvitationView = party.InvitationSnapshot

// ApplyPartyCommand（执行组队命令）所有命令先绑定主体和内容摘要，再在原子事务内校验权限与版本。
func (s *Service) ApplyPartyCommand(ctx context.Context, actorPlayerID string, command PartyCommand) (*party.Party, error) {
	if actorPlayerID == "" || command.PartyID == "" || command.ClientRequestID == "" || command.ExpectedRevision < 0 ||
		len(actorPlayerID) > 128 || len(command.PartyID) > 128 || len(command.ClientRequestID) > 128 || len(command.TargetPlayerID) > 128 || len(command.MembershipRequestID) > 128 || len(command.Action) > 32 {
		return nil, errors.New("PARTY_COMMAND_INVALID: 命令身份、长度或版本无效")
	}
	atomic, ok := s.parties.(AtomicPartyRepository)
	if !ok {
		return nil, errors.New("PARTY_ATOMIC_STORE_REQUIRED: 未装配事务仓储")
	}
	now := time.Now().UTC()
	return atomic.Update(ctx, command.PartyID, command.ExpectedRevision,
		scopedOperationKey(actorPlayerID, "party", command.ClientRequestID), fingerprintOf(command), func(value *party.Party) error {
			switch command.Action {
			case "invite":
				return value.Invite(actorPlayerID, command.MembershipRequestID, command.TargetPlayerID, now)
			case "requestJoin":
				return value.RequestJoin(command.MembershipRequestID, actorPlayerID, now)
			case "acceptRequest", "declineRequest":
				return value.ResolveMembershipRequest(actorPlayerID, command.MembershipRequestID, command.Action == "acceptRequest", now)
			case "cancelRequest":
				return value.CancelMembershipRequest(actorPlayerID, command.MembershipRequestID)
			case "ready":
				return value.SetReady(actorPlayerID, command.Ready)
			case "transferLeader":
				return value.TransferLeadership(actorPlayerID, command.TargetPlayerID)
			case "leave":
				return value.Leave(actorPlayerID)
			case "disband":
				return value.Disband(actorPlayerID)
			case "kick":
				if actorPlayerID != value.LeaderPlayerID {
					return errors.New("PARTY_NOT_LEADER: 只有队长可以移除成员")
				}
				if command.TargetPlayerID == "" {
					return errors.New("PARTY_COMMAND_INVALID: 移除目标不能为空")
				}
				return value.RemoveMember(command.TargetPlayerID)
			default:
				return errors.New("PARTY_COMMAND_INVALID: 未注册的组队命令")
			}
		})
}

// GetCurrentParty（读取本人队伍）索引返回的对象仍需复核成员关系，防止过期索引泄露其他队伍。
func (s *Service) GetCurrentParty(ctx context.Context, actorPlayerID string) (*party.Party, error) {
	atomic, ok := s.parties.(AtomicPartyRepository)
	if !ok || strings.TrimSpace(actorPlayerID) == "" {
		return nil, errors.New("PARTY_ATOMIC_STORE_REQUIRED: 主体或仓储无效")
	}
	value, err := atomic.GetByPlayerID(ctx, actorPlayerID)
	if err != nil {
		return nil, err
	}
	if _, found := value.Members[actorPlayerID]; !found {
		return nil, ErrPartyNotFound
	}
	return value, nil
}

// GetPartyInvitations（读取本人邀请）使用服务端时间过滤，不信任客户端的到期判断。
func (s *Service) GetPartyInvitations(ctx context.Context, actorPlayerID string) ([]PartyInvitationView, error) {
	atomic, ok := s.parties.(AtomicPartyRepository)
	if !ok || actorPlayerID == "" {
		return nil, errors.New("PARTY_ATOMIC_STORE_REQUIRED: 主体或仓储无效")
	}
	return atomic.GetInvitations(ctx, actorPlayerID, time.Now().UTC())
}

// ReadPartyState（本人授权快照）仅无队伍错误转换为HasParty=false，存储／迁移失败必须传播。
func (s *Service) ReadPartyState(ctx context.Context, actorPlayerID string) (party.Snapshot, error) {
	value, err := s.GetCurrentParty(ctx, actorPlayerID)
	if errors.Is(err, ErrPartyNotFound) {
		return party.EmptySnapshot(time.Now()), nil
	}
	if err != nil {
		return party.Snapshot{}, err
	}
	return value.SnapshotFor(actorPlayerID, time.Now())
}

// CommandParty（公开命令投影）不把聚合中的其他邀请、申请或内部字段直接序列化给客户端。
func (s *Service) CommandParty(ctx context.Context, actorPlayerID string, command party.Command) (party.CommandResult, error) {
	value, err := s.ApplyPartyCommand(ctx, actorPlayerID, command)
	if err != nil {
		return party.CommandResult{}, err
	}
	view := party.EmptySnapshot(time.Now())
	if _, member := value.Members[actorPlayerID]; member {
		view, err = value.SnapshotFor(actorPlayerID, time.Now())
		if err != nil {
			return party.CommandResult{}, err
		}
	}
	return party.CommandResult{ClientRequestID: command.ClientRequestID, AffectedPartyID: value.ID, AffectedRevision: value.Revision, Snapshot: view}, nil
}

// GetByPlayerID（测试用事务仓储查询）仅供单进程测试／明确本地配置，不替代生产Redis。
func (r *MemoryPartyRepository) GetByPlayerID(_ context.Context, playerID string) (*party.Party, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	value := r.items[r.players[playerID]]
	if value == nil {
		return nil, ErrPartyNotFound
	}
	return value.Clone(), nil
}

// Update（测试用原子更新）复制后执行并检查跨队唯一索引，失败不提交任何字段或回执。
func (r *MemoryPartyRepository) Update(ctx context.Context, partyID string, expectedRevision int64, key, fingerprint string, mutation func(*party.Party) error) (*party.Party, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if receipt, exists := r.receipts[key]; key != "" && exists {
		if receipt.Fingerprint != fingerprint {
			return nil, errors.New("PARTY_IDEMPOTENCY_CONFLICT: 同一命令对应不同内容")
		}
		return receipt.Result.Clone(), nil
	}
	current := r.items[partyID]
	if current == nil {
		return nil, ErrPartyNotFound
	}
	if current.Revision != expectedRevision {
		return nil, errors.New("PARTY_REVISION_CONFLICT: 队伍已变化")
	}
	next := current.Clone()
	if err := mutation(next); err != nil {
		return nil, err
	}
	for id := range next.Members {
		if owner := r.players[id]; owner != "" && owner != partyID {
			return nil, errors.New("PARTY_ALREADY_MEMBER: 玩家已在其他队伍")
		}
	}
	for id := range current.Members {
		if _, retained := next.Members[id]; !retained {
			delete(r.players, id)
		}
	}
	for id := range next.Members {
		r.players[id] = partyID
	}
	r.items[partyID] = next.Clone()
	r.publishPartyChangesLocked(current, next)
	if key != "" {
		r.receipts[key] = PartyOperationReceipt{Fingerprint: fingerprint, Result: next.Clone()}
	}
	return next.Clone(), nil
}

// GetInvitations（测试收件箱）按当前真实请求过滤，最多20条；不生成虚拟在线玩家。
func (r *MemoryPartyRepository) GetInvitations(_ context.Context, playerID string, now time.Time) ([]PartyInvitationView, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	result := []PartyInvitationView{}
	for _, value := range r.items {
		for _, request := range value.MembershipRequests {
			if request.Kind == party.MembershipInvitation && request.PlayerID == playerID && request.State == party.MembershipRequestPending && now.Before(request.ExpiresAtUTC) {
				result = append(result, PartyInvitationView{PartyID: value.ID, RequestID: request.ID, InviterPlayerID: request.RequestedByPlayerID, MemberCount: len(value.Members), Revision: value.Revision, ExpiresAtUTC: request.ExpiresAtUTC})
				if len(result) == 20 {
					return result, nil
				}
			}
		}
	}
	return result, nil
}
