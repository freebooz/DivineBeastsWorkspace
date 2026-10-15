// 本文件声明普通队伍的可恢复观察合同；传输层按认证主体读取，游标只标识本人事件进度，不授予任何权限。
package party

import "time"

// Observation（本人观察快照）首次及重同步返回完整授权状态；空等待仅Changed=false，不代表无队伍或业务取消。
type Observation struct {
	ScopeID        string               `json:"scopeId"`        // 本人普通组队作用域；客户端另外绑定认证代次。
	Cursor         string               `json:"cursor"`         // 有界事件日志游标，不允许跨玩家复用。
	ResyncRequired bool                 `json:"resyncRequired"` // 游标不在保留窗口时必须替换完整显示。
	Changed        bool                 `json:"changed"`        // 本次发生数据变化或权威截止时间到达。
	ServerTimeUTC  time.Time            `json:"serverTimeUtc"`  // UTC显示校时；不授予客户端判定终态的权力。
	Snapshot       Snapshot             `json:"snapshot"`       // 当前认证主体的队伍状态。
	Invitations    []InvitationSnapshot `json:"invitations"`    // 本人尚未到期邀请，最多20条。
}
