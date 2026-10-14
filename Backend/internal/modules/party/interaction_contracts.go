// 本文件定义普通队伍命令与授权只读快照，供Gateway与Match端口共用；JSON字段对应Shared/Contracts真源。
// 不包含认证令牌、内部仓储索引或匹配隐藏分；Revision以十进制字符串跨UE/Go传输，避免JSON浮点精度丢失。
package party

import (
	"errors"
	"sort"
	"time"
)

// Command（组队用户意图）Actor身份由认证上下文单独传入，不能写入请求正文冒名。
type Command struct {
	PartyID             string `json:"partyId"`                       // 目标队伍，不构成权限证明。
	Action              string `json:"action"`                        // 白名单命令名。
	TargetPlayerID      string `json:"targetPlayerId,omitempty"`      // 受控目标玩家。
	MembershipRequestID string `json:"membershipRequestId,omitempty"` // 邀请／申请身份。
	ClientRequestID     string `json:"clientRequestId"`               // 主体域内幂等标识。
	ExpectedRevision    int64  `json:"expectedRevision,string"`       // 已读取的队伍版本。
	Ready               bool   `json:"ready,omitempty"`               // ready操作的本人准备状态。
}

// MemberSnapshot（成员公开视图）不复制内部加入序号；权限以服务器给出的本人角色为准。
type MemberSnapshot struct {
	PlayerID    string `json:"playerId"`    // 成员公开身份。
	DisplayName string `json:"displayName"` // 服务器昵称，不参与授权。
	Ready       bool   `json:"ready"`       // 本人对当前名单版本的准备状态。
	Online      bool   `json:"online"`      // 可信在线状态，不由客户端推断。
}

// RequestSnapshot（授权请求视图）只对当前队长或该请求参与者发布。
type RequestSnapshot struct {
	ID           string                 `json:"requestId"`    // 可处理请求身份。
	Kind         MembershipRequestKind  `json:"kind"`         // 邀请／申请类型。
	PlayerID     string                 `json:"playerId"`     // 希望入队的玩家。
	State        MembershipRequestState `json:"state"`        // 已提交终态；Pending仍须按ExpiresAtUTC校验。
	ExpiresAtUTC time.Time              `json:"expiresAtUtc"` // UTC到期时间。
}

// Snapshot（本人普通队伍快照）HasParty=false是真实无队伍状态，不是服务故障或伪造队伍。
type Snapshot struct {
	HasParty       bool              `json:"hasParty"`        // 当前认证玩家是否在队伍中。
	PartyID        string            `json:"partyId"`         // 无队伍时空。
	LeaderPlayerID string            `json:"leaderPlayerId"`  // 当前队长。
	Revision       int64             `json:"revision,string"` // 队伍修订，无队伍为0。
	RosterLocked   bool              `json:"rosterLocked"`    // 后端名单锁。
	Members        []MemberSnapshot  `json:"members"`         // 最多5人；无队伍为空数组。
	Requests       []RequestSnapshot `json:"requests"`        // 经权限过滤的有界请求。
	ServerTimeUTC  time.Time         `json:"serverTimeUtc"`   // 客户端倒计时校时基准，不作为权威到期判定。
}

// CommandResult（提交回执）受影响队伍版本与本人当前队伍分别表达，离队／申请不会泄露目标队伍完整名单。
type CommandResult struct {
	ClientRequestID  string   `json:"clientRequestId"`         // 回传本命令标识，供界面校验旧回调。
	AffectedPartyID  string   `json:"affectedPartyId"`         // 本命令实际操作队伍。
	AffectedRevision int64    `json:"affectedRevision,string"` // 已提交的受影响修订。
	Snapshot         Snapshot `json:"snapshot"`                // 本人仍是成员时的授权快照，否则为空队伍快照。
}

// InvitationSnapshot（本人邀请收件项）只展示本人请求，不公开其他申请者及完整队伍。
type InvitationSnapshot struct {
	PartyID         string    `json:"partyId"`         // 来源队伍。
	RequestID       string    `json:"requestId"`       // 本人可处理的请求。
	InviterPlayerID string    `json:"inviterPlayerId"` // 邀请者公开身份。
	MemberCount     int       `json:"memberCount"`     // 实际队伍人数。
	Revision        int64     `json:"revision,string"` // 接受命令的队伍版本。
	ExpiresAtUTC    time.Time `json:"expiresAtUtc"`    // UTC到期时间。
}

// EmptySnapshot（真实无队伍视图）显式输出空数组，不表示网络、仓储或权限故障成功。
func EmptySnapshot(now time.Time) Snapshot {
	return Snapshot{Members: []MemberSnapshot{}, Requests: []RequestSnapshot{}, ServerTimeUTC: now.UTC()}
}

// SnapshotFor（授权投影）只供真实成员读取，按稳定成员身份排序展示但不据此选队长。
func (p *Party) SnapshotFor(actorPlayerID string, now time.Time) (Snapshot, error) {
	if _, member := p.Members[actorPlayerID]; !member || actorPlayerID == "" {
		return Snapshot{}, errors.New("PARTY_NOT_MEMBER: 无权读取此队伍")
	}
	view := EmptySnapshot(now)
	view.HasParty = true
	view.PartyID = p.ID
	view.LeaderPlayerID = p.LeaderPlayerID
	view.Revision = p.Revision
	view.RosterLocked = p.RosterLocked
	for _, member := range p.Members {
		view.Members = append(view.Members, MemberSnapshot{PlayerID: member.PlayerID, DisplayName: member.DisplayName, Ready: member.Ready, Online: member.Online})
	}
	sort.Slice(view.Members, func(i, j int) bool { return view.Members[i].PlayerID < view.Members[j].PlayerID })
	for _, request := range p.MembershipRequests {
		if actorPlayerID == p.LeaderPlayerID || actorPlayerID == request.PlayerID || actorPlayerID == request.RequestedByPlayerID {
			view.Requests = append(view.Requests, RequestSnapshot{ID: request.ID, Kind: request.Kind, PlayerID: request.PlayerID, State: request.State, ExpiresAtUTC: request.ExpiresAtUTC})
		}
	}
	sort.Slice(view.Requests, func(i, j int) bool { return view.Requests[i].ID < view.Requests[j].ID })
	return view, nil
}
