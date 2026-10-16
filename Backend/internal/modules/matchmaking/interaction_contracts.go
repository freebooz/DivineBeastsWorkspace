// 本文件是匹配命令和本人只读观察DTO，字段唯一真源在Shared/OpenAPI；不含迁移票据或服务器地址。
package matchmaking

import (
	"encoding/json"
	"errors"
	"time"
)

// Command只表示用户意图；认证主体、会话和角色由Gateway另行注入，禁止在正文冒名。
type Command struct {
	Action           string `json:"action"`                  // start/cancel/reply/ready；不同阶段权限分别验证。
	ClientRequestID  string `json:"clientRequestId"`         // 主体/操作域幂等键，重试沿用。
	ExpectedRevision int64  `json:"expectedRevision,string"` // start为队伍版本；solo start为0，其余为对应聚合版本。
	TicketID         string `json:"ticketId,omitempty"`      // 取消或准备的当前票据身份。
	CandidateID      string `json:"candidateId,omitempty"`   // 逐人确认身份，不是入场资格。
	ArenaModeID      string `json:"arenaModeId,omitempty"`   // start的模式定义身份。
	PartyID          string `json:"partyId,omitempty"`       // 已组队必须提供本人实际队伍，单人可为空。
	RegionID         string `json:"regionId,omitempty"`      // start区域分区。
	Accept           bool   `json:"accept"`                  // ready/reply时的本人接受意图。
}

// UnmarshalJSON防止缺失accept被Go零值解释为主动拒绝；重复键/未知字段仍由外层严格对象解码器拒绝。
func (c *Command) UnmarshalJSON(payload []byte) error {
	type plain Command
	var next plain
	if err := json.Unmarshal(payload, &next); err != nil {
		return err
	}
	if next.Action == "ready" || next.Action == "reply" {
		var fields map[string]json.RawMessage
		if err := json.Unmarshal(payload, &fields); err != nil {
			return err
		}
		raw, found := fields["accept"]
		if !found || string(raw) == "null" {
			return errors.New("MATCH_COMMAND_INVALID: 回复必须明确accept")
		}
	}
	*c = Command(next)
	return nil
}

// ActorContext只在可信服务间持久，权限由Gateway从会话和已选角色验证；不包含令牌或用户密码。
type ActorContext struct {
	PlayerID           string `json:"playerId"`                 // 已认证玩家。
	SessionID          string `json:"sessionId"`                // 当前认证会话，取迁移时必须相同。
	CharacterID        string `json:"characterId"`              // 后端已选择并验证归属的持久角色。
	HeroDefinitionID   string `json:"heroDefinitionId"`         // 后端角色对应英雄身份，不能由客户端覆盖。
	CharacterRevision  int64  `json:"characterRevision,string"` // 读取角色的可信修订。
	SourceGameServerID string `json:"sourceGameServerId"`       // 控制面确认的来源实例，不能接受客户端地址。
	SourceExperienceID string `json:"sourceExperienceId"`       // 控制面确认来源体验，赛后重新分配，不复用旧地址。
}

// ParticipantSnapshot只公开候选名单、分队和明确回复；不公开SessionID、隐藏分或其他玩家的准入票据。
type ParticipantSnapshot struct {
	PlayerID string `json:"playerId"` // 本轮参与者公开身份。
	TeamID   string `json:"teamId"`   // 候选确定前为空。
	Ready    bool   `json:"ready"`    // 本轮同模式准备事实。
	Accepted bool   `json:"accepted"` // 找到比赛后逐人确认事实。
}

// PlayerSnapshot是本人当前报名状态；idle只用于服务器确认无有效记录，故障不能转换为idle成功。
type PlayerSnapshot struct {
	ServiceReady      bool                  `json:"serviceReady"`             // 真实内容与编排器均已装配才允许新报名。
	Development       bool                  `json:"development"`              // 开发内容明确标记，不代表正式发行。
	RegionID          string                `json:"regionId"`                 // 部署认可的默认或当前队列区域，不由UI猜测。
	State             string                `json:"state"`                    // 明确生命周期阶段。
	TicketID          string                `json:"ticketId"`                 // 当前或最近本轮报名身份。
	PartyID           string                `json:"partyId"`                  // 原子普通队伍，solo为空。
	ArenaModeID       string                `json:"arenaModeId"`              // 服务器认可模式。
	CandidateID       string                `json:"candidateId"`              // 尚未形成候选时为空。
	Revision          int64                 `json:"revision,string"`          // 对应票据修订。
	CandidateRevision int64                 `json:"candidateRevision,string"` // 确认命令需使用候选修订。
	DeadlineUTC       *time.Time            `json:"deadlineUtc"`              // 无倒计时为null；UI按服务器时间作视觉动画。
	ServerTimeUTC     time.Time             `json:"serverTimeUtc"`            // UTC校时，不授予客户端判断终态的权力。
	CanCancel         bool                  `json:"canCancel"`                // 服务器投影的本人整队取消资格，写命令仍复核。
	CanReply          bool                  `json:"canReply"`                 // 本人是否还有未回复的有效候选。
	CanReady          bool                  `json:"canReady"`                 // 当前是否能提交本人限时准备。
	ReasonCode        string                `json:"reasonCode"`               // 安全稳定原因，不含任意内部正文。
	Participants      []ParticipantSnapshot `json:"participants"`             // 最多10个授权可见成员。
}

// Observation在独立匹配作用域续接同一仓储事件日志；初始和失效游标均提供完整快照。
type Observation struct {
	ScopeID        string         `json:"scopeId"`        // matchmaking/本人ID，客户端还须绑定认证代次。
	Cursor         string         `json:"cursor"`         // 已投影事件进度。
	ResyncRequired bool           `json:"resyncRequired"` // 失效游标要求全量替换。
	Changed        bool           `json:"changed"`        // 真变化或阶段截止时间到达。
	Snapshot       PlayerSnapshot `json:"snapshot"`       // 本人安全状态，不含迁移秘密。
}

// MigrationRequest只选择本人比赛和方向，永远不接受客户端目标地址/ServerID/票据内容。
type MigrationRequest struct {
	CandidateID     string `json:"candidateId"`     // 本人已确认候选。
	Direction       string `json:"direction"`       // arena或return。
	ClientRequestID string `json:"clientRequestId"` // 同一逻辑迁移重试沿用。
}

// Migration仅交给Flow/Loading/Session，不写UI状态或日志；字段沿用已有世界进入合同。
type Migration struct {
	AssignmentID    string `json:"assignmentId"`    // 已确认分配。
	GameServerID    string `json:"gameServerId"`    // 控制面选择实例。
	ServerRoleID    string `json:"serverRoleId"`    // 三个正式角色之一。
	ExperienceID    string `json:"experienceId"`    // 与角色匹配体验。
	WorldID         string `json:"worldId"`         // 合法逻辑世界。
	MapID           string `json:"mapId"`           // 已装配地图定义，不是客户端可填地址。
	RegionID        string `json:"regionId"`        // 已配置区域。
	TicketID        string `json:"ticketId"`        // 本次短期准入身份。
	CharacterID     string `json:"characterId"`     // 本局锁定角色。
	SessionID       string `json:"sessionId"`       // 当前认证会话。
	GameSessionID   string `json:"gameSessionId"`   // 单次目标绑定。
	ServerBootID    string `json:"serverBootId"`    // 实例真实启动代次。
	ProtocolVersion uint32 `json:"protocolVersion"` // 真实网络协议版本。
	SessionEpoch    uint64 `json:"sessionEpoch"`    // 权威防旧代次。
	Endpoint        string `json:"endpoint"`        // 敏感连接目标，仅传输层消费。
	TransferTicket  string `json:"transferTicket"`  // 短期一次性秘密，禁止日志/URL/公共投影。
}
