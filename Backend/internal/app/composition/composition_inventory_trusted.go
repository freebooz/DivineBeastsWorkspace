//go:build productiondeps

package composition

// PlayerData可信库存事件装配根：本地/开发构建不启动GameServer发奖入口。
// 必须同时设置独立内部凭据、固定GameID和只读后端物品策略文件，
// 并已执行000003、000008～000011相关数据库迁移。
// 默认未配置时安全关闭，普通Gateway和玩家数据服务仍可提供背包整理业务。

import (
	"errors"
	"log/slog"
	"os"
	"strings"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/platform/database/postgres"
)

// configureTrustedInventory（启用经准入租约和可信事实双重验证的背包事件）。
// secret用于内网HTTP服务身份，不能复用Gateway、登录或GameServerControl的Token。
// 返回nil processor表示已明确禁用，不得以默认全放行验证器替代。
func configureTrustedInventory(
	playerService *playerdata.Service, pool *postgres.Pool,
) (*inventory.TrustedEventProcessor, string, error) {
	secret := strings.TrimSpace(os.Getenv("INVENTORY_TRUSTED_EVENTS_TOKEN"))
	if secret == "" {
		slog.Warn("可信库存游戏服事件入口未配置，拾取、任务奖励及使用仍安全关闭")
		return nil, "", nil
	}
	gameID := strings.TrimSpace(os.Getenv("INVENTORY_GAME_ID"))
	policyPath := strings.TrimSpace(os.Getenv("INVENTORY_POLICY_FILE"))
	if len(secret) < 32 || gameID == "" || policyPath == "" ||
		secret == strings.TrimSpace(os.Getenv("GAMESERVERCONTROL_INTERNAL_TOKEN")) {
		return nil, "", errors.New("库存可信事件必须具有独立强令牌、固定游戏ID及受控物品规则文件")
	}
	file, err := os.Open(policyPath)
	if err != nil {
		return nil, "", err
	}
	defer file.Close()
	catalog, err := inventory.LoadItemPolicyCatalog(file, gameID)
	if err != nil {
		return nil, "", err
	}
	authority := postgres.NewInventoryAuthorityRepository(pool)
	grantAuth, err := inventory.NewVerifiedGrantAuthorizer(authority, authority, catalog)
	if err != nil {
		return nil, "", err
	}
	consumeAuth := &inventory.VerifiedConsumeAuthorizer{
		Servers: authority, Evidence: authority, Catalog: catalog,
	}
	trustedService := inventory.NewTrustedGrantService(
		postgres.NewInventoryRepository(pool), grantAuth, consumeAuth)
	processor, err := inventory.NewTrustedEventProcessor(
		gameID, authority, authority, trustedService)
	if err != nil {
		return nil, "", err
	}
	playerService.AttachTrustedInventory(trustedService)
	return processor, secret, nil
}
