// 本文件实现平台层普通队伍邀请／申请的纯领域规则，由Match应用在仓储事务中调用。
// 服务器提供时间和身份；领域对象不联网、不拥有认证，不替代Redis跨队唯一索引或事件发件箱。
package party

import (
	"errors"
	"strings"
	"time"
	"unicode"
)

// MembershipRequestKind（入队请求来源）区分队长邀请与玩家申请，不增加竞技战队实体。
type MembershipRequestKind string

const (
	MembershipInvitation  MembershipRequestKind = "invitation"  // 队长向目标玩家发送邀请。
	MembershipApplication MembershipRequestKind = "application" // 玩家向队伍提出申请。
)

// MembershipRequestState（请求终态）一旦离开Pending不能反转；到期另由服务器时间校验。
type MembershipRequestState string

const (
	MembershipRequestPending   MembershipRequestState = "pending"   // 尚未处理，不预占名额。
	MembershipRequestAccepted  MembershipRequestState = "accepted"  // 已在同一事务批准入队。
	MembershipRequestDeclined  MembershipRequestState = "declined"  // 目标／队长拒绝。
	MembershipRequestCancelled MembershipRequestState = "cancelled" // 发起人撤回或队伍解散。
)

const (
	InvitationLifetime           = 60 * time.Second  // InvitationLifetime（服务器邀请有效期）。
	ApplicationLifetime          = 120 * time.Second // ApplicationLifetime（服务器申请有效期）。
	MaxPendingMembershipRequests = 20                // 单队未到期请求上限，防止无限增长和邀请洪泛。
	MaxMembershipRequestRecords  = 64                // 含近期终态的最大记录数；历史过期记录在插入时清理。
)

// MembershipRequest（邀请或申请）属于当前Party；不能单凭ID推断读取／批准权限。
type MembershipRequest struct {
	ID                  string                 // ID（命令携带的请求身份），服务端校验同ID同意图；不是授权令牌。
	Kind                MembershipRequestKind  // Kind（权限判定所需请求类型）。
	PlayerID            string                 // PlayerID（希望加入队伍的玩家）。
	RequestedByPlayerID string                 // RequestedByPlayerID（原始邀请者或申请者），只用于审计与撤回。
	ExpiresAtUTC        time.Time              // ExpiresAtUTC（UTC绝对到期时间）；等于当前时刻即已失效。
	State               MembershipRequestState // State（已提交终态），客户端不能直接赋值。
}

// validMembershipID（验证逻辑标识）拒绝空白、控制字符和超长输入，不改变已发布ID的大小写。
func validMembershipID(value string) bool {
	return value != "" && len(value) <= 128 && strings.TrimSpace(value) == value && strings.IndexFunc(value, unicode.IsControl) < 0
}

// Invite（队长邀请）不预占成员名额；同ID同目标重试幂等，异参冲突，时间必须来自服务端。
func (p *Party) Invite(actorPlayerID, requestID, targetPlayerID string, now time.Time) error {
	if actorPlayerID != p.LeaderPlayerID || actorPlayerID == "" {
		return errors.New("PARTY_NOT_LEADER: 只有队长可以邀请")
	}
	return p.addMembershipRequest(MembershipRequest{ID: requestID, Kind: MembershipInvitation, PlayerID: targetPlayerID,
		RequestedByPlayerID: actorPlayerID, ExpiresAtUTC: now.UTC().Add(InvitationLifetime), State: MembershipRequestPending}, now)
}

// RequestJoin（申请加入）只登记本人申请；上层须从认证上下文传PlayerID，而非信任客户端声明。
func (p *Party) RequestJoin(requestID, playerID string, now time.Time) error {
	return p.addMembershipRequest(MembershipRequest{ID: requestID, Kind: MembershipApplication, PlayerID: playerID,
		RequestedByPlayerID: playerID, ExpiresAtUTC: now.UTC().Add(ApplicationLifetime), State: MembershipRequestPending}, now)
}

// addMembershipRequest（登记请求）所有失败检查在修改前完成；清理仅移除已到期历史，不能恢复旧终态。
func (p *Party) addMembershipRequest(request MembershipRequest, now time.Time) error {
	if !validMembershipID(request.ID) || !validMembershipID(request.PlayerID) || now.IsZero() {
		return errors.New("PARTY_REQUEST_INVALID: 标识或服务器时间无效")
	}
	if current, found := p.MembershipRequests[request.ID]; found {
		if current.Kind != request.Kind || current.PlayerID != request.PlayerID || current.RequestedByPlayerID != request.RequestedByPlayerID {
			return errors.New("PARTY_REQUEST_CONFLICT: 请求ID已对应其他意图")
		}
		return nil
	}
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 名单锁定时不能登记请求")
	}
	if p.LeaderPlayerID == "" || len(p.Members) == 0 {
		return errors.New("PARTY_NOT_FOUND: 队伍已解散")
	}
	if _, member := p.Members[request.PlayerID]; member {
		return errors.New("PARTY_ALREADY_MEMBER: 玩家已在本队")
	}
	if len(p.Members) >= MaxMembers {
		return errors.New("PARTY_FULL: 队伍已满")
	}
	active, retained := 0, 0
	for _, current := range p.MembershipRequests {
		if now.Before(current.ExpiresAtUTC) {
			retained++
			if current.State == MembershipRequestPending {
				active++
				if current.PlayerID == request.PlayerID && current.Kind == request.Kind {
					return errors.New("PARTY_REQUEST_PENDING: 已有该玩家的待处理请求")
				}
			}
		}
	}
	if active >= MaxPendingMembershipRequests || retained >= MaxMembershipRequestRecords {
		return errors.New("PARTY_REQUEST_LIMIT: 待处理或近期请求过多")
	}
	if p.MembershipRequests == nil {
		p.MembershipRequests = map[string]MembershipRequest{}
	}
	for id, current := range p.MembershipRequests {
		if !now.Before(current.ExpiresAtUTC) {
			delete(p.MembershipRequests, id)
		}
	}
	p.MembershipRequests[request.ID] = request
	p.Revision++
	return nil
}

// ResolveMembershipRequest（接受／拒绝）由邀请目标或当前队长执行；加入和请求终态必须在同一外部事务保存。
// 接受后的同意重试不再次加入，拒绝后的同意返回冲突；过期、满员、锁定和越权不改变请求状态。
func (p *Party) ResolveMembershipRequest(actorPlayerID, requestID string, accept bool, now time.Time) error {
	request, found := p.MembershipRequests[requestID]
	if !found {
		return errors.New("PARTY_REQUEST_NOT_FOUND: 请求不存在")
	}
	if request.Kind != MembershipInvitation && request.Kind != MembershipApplication {
		return errors.New("PARTY_REQUEST_INVALID: 请求类型无法识别")
	}
	if (request.Kind == MembershipInvitation && actorPlayerID != request.PlayerID) ||
		(request.Kind == MembershipApplication && actorPlayerID != p.LeaderPlayerID) || actorPlayerID == "" {
		return errors.New("PARTY_REQUEST_FORBIDDEN: 无权处理此请求")
	}
	wanted := MembershipRequestDeclined
	if accept {
		wanted = MembershipRequestAccepted
	}
	if request.State != MembershipRequestPending {
		if request.State == wanted {
			return nil
		}
		return errors.New("PARTY_REQUEST_TERMINAL: 请求已结束")
	}
	if now.IsZero() || !now.Before(request.ExpiresAtUTC) {
		return errors.New("PARTY_REQUEST_EXPIRED: 请求已经到期")
	}
	if accept {
		revision := p.Revision
		if err := p.AddMember(Member{PlayerID: request.PlayerID}); err != nil {
			return err
		}
		// 同一玩家可能经另一种请求先入队；AddMember幂等不改修订，但本次Pending终态变化仍必须可被订阅者观察。
		if p.Revision == revision {
			p.Revision++
		}
	} else {
		p.Revision++
	}
	request.State = wanted
	p.MembershipRequests[requestID] = request
	return nil
}

// CancelMembershipRequest（发起人撤回）只结束Pending，不移除已经接受的成员；邀请者失去队长权限后不能撤回。
func (p *Party) CancelMembershipRequest(actorPlayerID, requestID string) error {
	request, found := p.MembershipRequests[requestID]
	if !found {
		return errors.New("PARTY_REQUEST_NOT_FOUND: 请求不存在")
	}
	if request.Kind != MembershipInvitation && request.Kind != MembershipApplication {
		return errors.New("PARTY_REQUEST_INVALID: 请求类型无法识别")
	}
	if actorPlayerID == "" || actorPlayerID != request.RequestedByPlayerID ||
		(request.Kind == MembershipInvitation && actorPlayerID != p.LeaderPlayerID) {
		return errors.New("PARTY_REQUEST_FORBIDDEN: 无权撤回此请求")
	}
	if request.State == MembershipRequestCancelled {
		return nil
	}
	if request.State != MembershipRequestPending {
		return errors.New("PARTY_REQUEST_TERMINAL: 请求已结束")
	}
	request.State = MembershipRequestCancelled
	p.MembershipRequests[requestID] = request
	p.Revision++
	return nil
}

// cancelPendingMembershipRequests（解散清理）在同一聚合变更中使未完成请求失效，不逐项增加Revision。
func (p *Party) cancelPendingMembershipRequests() {
	for id, request := range p.MembershipRequests {
		if request.State == MembershipRequestPending {
			request.State = MembershipRequestCancelled
			p.MembershipRequests[id] = request
		}
	}
}
