//go:build productiondeps

package postgres

// 受控会话续租：只调用数据库000012已有Session绑定条件更新函数，
// 不允许HTTP客户端请求凭空创建绑定、重复签发转移票据或更改归属服务器。

import (
    "context"
    "time"

    "divinebeasts/backend/internal/modules/inventory"
)

var _ inventory.ServerLeaseRenewer = (*InventoryAuthorityRepository)(nil)

// RenewServerLease（续期同一真实玩家连接）最多30秒，失败关闭。
// PostgreSQL使用000003平台门禁锁并同时校验账号登出、实例健康和当前SessionEpoch。
func (r *InventoryAuthorityRepository) RenewServerLease(
    ctx context.Context, lease inventory.TrustedPlayerLease, seconds int32,
) (time.Time, error) {
    if err := ctx.Err(); err != nil { return time.Time{}, err }
    if seconds < 1 || seconds > inventory.MaxInventorySessionLeaseSeconds {
        return time.Time{}, inventoryTrustDenied()
    }
    if r == nil || r.pool == nil || r.pool.inner == nil {
        return time.Time{}, inventoryTrustDenied()
    }
    var authorityUntil time.Time
    err := r.pool.inner.QueryRow(ctx, `SELECT session_renew_inventory_player(
        $1,$2,$3,$4,$5,$6,$7)`,
        lease.GameID, lease.PlayerID, lease.GameServerID, lease.ServerBootID,
        lease.SessionEpoch, lease.ConnectionID, seconds).Scan(&authorityUntil)
    if err != nil {
        return time.Time{}, inventoryPGError(err)
    }
    return authorityUntil.UTC(), nil
}
