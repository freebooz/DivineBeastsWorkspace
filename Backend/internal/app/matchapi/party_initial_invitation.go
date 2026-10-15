// 本文件实现无队伍时的原子邀请：创建队伍、邀请、唯一成员索引与主体回执必须同一事务，不留下空队伍半成品。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
	"errors"
	"time"
)

// InitialPartyInvitationRepository由普通队伍仓储实现，不能用先Save再Update两次写代替。
type InitialPartyInvitationRepository interface {
	CreateInvitedParty(context.Context, *party.Party, string, string) (*party.Party, error)
}

// initialInvitation只允许空PartyID/初始版本0的邀请意图；同主体同键生成稳定队伍，重试不创建第二个队伍。
func (s *Service) initialInvitation(ctx context.Context, actor string, command party.Command) (*party.Party, error) {
	if command.Action != "invite" || command.ExpectedRevision != 0 || !validMatchID(command.TargetPlayerID) || !validMatchID(command.MembershipRequestID) {
		return nil, errors.New("PARTY_COMMAND_INVALID: 初始邀请缺少目标/身份或版本无效")
	}
	store, ok := s.parties.(InitialPartyInvitationRepository)
	if !ok {
		return nil, errors.New("PARTY_ATOMIC_STORE_REQUIRED: 未装配初始邀请事务")
	}
	id := "party-" + fingerprintOf([]string{actor, command.ClientRequestID})[:32]
	value := party.New(id, party.Member{PlayerID: actor, Online: true})
	if err := value.Invite(actor, command.MembershipRequestID, command.TargetPlayerID, time.Now().UTC()); err != nil {
		return nil, err
	}
	return store.CreateInvitedParty(ctx, value, scopedOperationKey(actor, "party", command.ClientRequestID), fingerprintOf(command))
}

// CreateInvitedParty是明确本地/测试仓储的同锁提交；任何冲突或取消都不能留下队伍/收件箱/回执子集。
func (r *MemoryPartyRepository) CreateInvitedParty(ctx context.Context, value *party.Party, key, fingerprint string) (*party.Party, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if old, found := r.receipts[key]; found {
		if old.Fingerprint != fingerprint || old.Result == nil {
			return nil, errors.New("PARTY_IDEMPOTENCY_CONFLICT: 同键异参")
		}
		return old.Result.Clone(), nil
	}
	if value == nil || value.ID == "" || len(value.Members) != 1 || len(value.MembershipRequests) != 1 {
		return nil, errors.New("PARTY_COMMAND_INVALID: 初始邀请聚合无效")
	}
	if _, found := r.items[value.ID]; found {
		return nil, errors.New("PARTY_EXISTS: 队伍身份已存在")
	}
	for player := range value.Members {
		if r.players[player] != "" {
			return nil, errors.New("PARTY_ALREADY_MEMBER: 玩家已在队伍")
		}
		if r.queue != nil && r.queue.Reservations[player] != "" {
			return nil, errors.New("MATCH_PLAYER_RESERVED: 玩家已有活动匹配")
		}
	}
	for _, request := range value.MembershipRequests {
		count := 0
		for _, old := range r.items {
			for _, pending := range old.MembershipRequests {
				if pending.PlayerID == request.PlayerID && pending.Kind == party.MembershipInvitation && pending.State == party.MembershipRequestPending && time.Now().UTC().Before(pending.ExpiresAtUTC) {
					count++
				}
			}
		}
		if count >= 20 {
			return nil, errors.New("PARTY_INBOX_FULL: 邀请收件箱已满")
		}
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	r.items[value.ID] = value.Clone()
	for player := range value.Members {
		r.players[player] = value.ID
	}
	r.receipts[key] = PartyOperationReceipt{Fingerprint: fingerprint, Result: value.Clone()}
	r.publishPartyChangesLocked(nil, value)
	return value.Clone(), nil
}
