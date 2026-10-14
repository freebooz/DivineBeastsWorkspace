package inventory

// 服务器玩家连接续租的可复用端口。不是授权签发接口，也不是公网Gateway能力。
// 唯有已经由平台session_reserve→claim→commit真正入库的会话能续租，
// 同时需要有效的在线刷新会话、未禁用账号及健康服务器实例。

import (
    "context"
    "time"
)

const (
    // MaxInventorySessionLeaseSeconds（单次续租最长秒数）匹配000003的在线权威门禁。
    MaxInventorySessionLeaseSeconds int32 = 30
)

// ServerLeaseRenewer（已验证Dedicated Server玩家租约续期端口）。
// 绑定不存在、租约已经过期、连接世代改变、登出/账号禁用时不得自动复活旧绑定。
type ServerLeaseRenewer interface {
    RenewServerLease(context.Context, TrustedPlayerLease, int32) (time.Time, error)
}
