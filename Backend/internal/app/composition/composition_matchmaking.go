// 本文件是五入口共享的匹配运行装配；配置默认关闭，真实内容/规则缺失时拒绝启用，不使用测试默认值冒充可用。
package composition

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/app/matchservice"
	"divinebeasts/backend/internal/platform/config"
	"errors"
	"log/slog"
	"strconv"
	"strings"
)

// configuredMatchmakingSettings读取部署显式配置；本地开发地图必须设置Development且不能自动升级ProductionReady。
func configuredMatchmakingSettings() (matchapi.MatchmakingSettings, error) {
	enabled := config.Getenv("MATCHMAKING_ENABLED", "false")
	development := config.Getenv("MATCHMAKING_DEVELOPMENT", "false")
	if enabled != "true" && enabled != "false" || development != "true" && development != "false" {
		return matchapi.MatchmakingSettings{}, errors.New("MATCH_CONFIG_INVALID: 布尔配置必须为true/false")
	}
	value := matchapi.MatchmakingSettings{Enabled: enabled == "true", Development: development == "true", MapID: strings.TrimSpace(config.Getenv("MATCHMAKING_MAP_ID", "")), ContentRevision: strings.TrimSpace(config.Getenv("MATCHMAKING_CONTENT_REVISION", "")), AllowedRegions: config.GetenvCSV("MATCHMAKING_ALLOWED_REGIONS")}
	if !value.Enabled {
		return value, nil
	}
	rule, err := strconv.ParseInt(config.Getenv("MATCHMAKING_RULE_REVISION", ""), 10, 32)
	if err != nil || rule < 1 {
		return value, errors.New("MATCH_CONFIG_INVALID: 必须提供实际规则修订")
	}
	hero, err := strconv.ParseInt(config.Getenv("MATCHMAKING_HERO_CATALOG_REVISION", ""), 10, 32)
	if err != nil || hero < 1 {
		return value, errors.New("MATCH_CONFIG_INVALID: 必须提供实际英雄目录修订")
	}
	value.ProjectRuleRevision = int32(rule)
	value.HeroCatalogRevision = int32(hero)
	if value.MapID == "" || value.ContentRevision == "" || len(value.AllowedRegions) == 0 {
		return value, errors.New("MATCH_CONTENT_NOT_CONFIGURED: 真实地图/内容/区域未配置")
	}
	return value, nil
}

// attachMatchRuntime只在启动前连接真正仓储和控制端口；页面/请求不能替换运行器。
func attachMatchRuntime(service *matchapi.Service, settings matchapi.MatchmakingSettings, store matchservice.OperationStore, control matchservice.RuntimeControl, nextID func(string) string) error {
	if err := service.ConfigureMatchmaking(settings); err != nil {
		return err
	}
	if !settings.Enabled {
		return nil
	}
	runtime, err := matchservice.NewRuntime(store, service, control, matchservice.RuntimeConfig{MapID: settings.MapID, ContentRevision: settings.ContentRevision, RuleRevision: settings.ProjectRuleRevision, HeroCatalogRevision: settings.HeroCatalogRevision}, nextID)
	if err != nil {
		return err
	}
	return service.AttachMatchmakingRuntime(runtime)
}

// runMatchWorkerAndHost把消费者和监听器置于同一生命周期；消费者永久退出会停止入口，避免接受无人处理的新报名。
func runMatchWorkerAndHost(ctx context.Context, service *matchapi.Service, enabled bool, host func(context.Context) error) error {
	if !enabled {
		return host(ctx)
	}
	owned, cancel := context.WithCancel(ctx)
	defer cancel()
	workerDone := make(chan error, 1)
	go func() {
		err := service.RunConfiguredMatchmaking(owned, func(err error) {
			code := strings.SplitN(err.Error(), ":", 2)[0]
			if len(code) > 80 || strings.ContainsAny(code, " \n\r") {
				code = "MATCH_STEP_RETRY"
			}
			slog.Warn("比赛持久步骤等待恢复", "code", code)
		})
		workerDone <- err
		cancel()
	}()
	hostErr := host(owned)
	cancel()
	workerErr := <-workerDone
	if ctx.Err() != nil {
		return hostErr
	}
	if workerErr != nil && !errors.Is(workerErr, context.Canceled) {
		return workerErr
	}
	return hostErr
}
