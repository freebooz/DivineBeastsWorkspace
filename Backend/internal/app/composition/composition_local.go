//go:build !productiondeps

// Package composition（应用装配）是五个cmd薄入口的唯一Composition Root（装配根）。
// 本文件提供本地/测试依赖；生产依赖由productiondeps构建文件替换。
package composition

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"log/slog"
	"strings"
	"time"

	generatedgp "divinebeasts/backend/generated/gameplatform"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/app/servicehost"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/identity"
	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/modules/servertransfer"
	"divinebeasts/backend/internal/platform/config"
	"divinebeasts/backend/internal/platform/outbox"
	"divinebeasts/backend/internal/transport/httpadapter"
)

// RunGateway（运行统一接入服务）通过真实HTTP客户端调用Identity/PlayerData/MatchService。
func RunGateway(ctx context.Context, cfg config.ServiceConfig) error {
	identityClient := httpadapter.NewIdentityClient(httpadapter.ClientConfig{BaseURL: config.Getenv("IDENTITY_SERVICE_URL", "http://127.0.0.1:8081")})
	playerDataClient := httpadapter.NewPlayerDataClient(httpadapter.ClientConfig{BaseURL: config.Getenv("PLAYER_DATA_SERVICE_URL", "http://127.0.0.1:8082")})
	matchClient := httpadapter.NewMatchClient(httpadapter.ClientConfig{BaseURL: config.Getenv("MATCH_SERVICE_URL", "http://127.0.0.1:8083")})
	internalToken := strings.TrimSpace(config.Getenv("GAMESERVERCONTROL_INTERNAL_TOKEN", ""))
	if internalToken == "" {
		return errors.New("GAMESERVERCONTROL_INTERNAL_TOKEN不能为空")
	}
	worldEntryClient := httpadapter.NewGameServerControlClient(httpadapter.GameServerControlClientConfig{
		ClientConfig:  httpadapter.ClientConfig{BaseURL: config.Getenv("GAMESERVERCONTROL_SERVICE_URL", "http://127.0.0.1:8084")},
		BearerToken:   internalToken,
		DefaultRegion: config.Getenv("GAME_DEFAULT_REGION", "us-west"),
	})
	handler := gateway.NewAPI(gateway.Config{ContractVersion: generatedgp.ContractVersion}, identityClient, playerDataClient, matchClient, matchClient, worldEntryClient)
	return servicehost.Run(ctx, cfg, handler)
}

// RunIdentity（运行身份服务）默认保留旧游客开发模式；仅当本地开发账号环境变量成对提供时，
// 使用正式密码领域服务 + 本地内存PersistentRepository，复用bcrypt、Token摘要和刷新/退出语义。
// productiondeps构建不包含本地内存仓储，也不读取这组开发账号变量。
func RunIdentity(ctx context.Context, cfg config.ServiceConfig) error {
	devAccount := strings.TrimSpace(config.Getenv("DIVINEBEASTS_DEV_LOGIN_USER", ""))
	devPassword := config.Getenv("DIVINEBEASTS_DEV_LOGIN_PASSWORD", "")
	if devAccount == "" && devPassword == "" {
		service := identity.NewService(identity.NewMemorySessionRepository(), identity.SystemClock{}, identity.CryptoTokenGenerator{}, 15*time.Minute, 30*24*time.Hour)
		return servicehost.Run(ctx, cfg, httpadapter.NewIdentityHandler(service))
	}
	if devAccount == "" || devPassword == "" {
		return errors.New("DIVINEBEASTS_DEV_LOGIN_USER与DIVINEBEASTS_DEV_LOGIN_PASSWORD必须成对设置")
	}

	repo := newLocalIdentityPersistentRepository()
	if err := seedLocalIdentityAccount(ctx, repo, "divine-beasts", devAccount, devPassword); err != nil {
		return err
	}
	service, err := identity.NewPersistentService(repo, identity.SystemClock{}, 15*time.Minute, 30*24*time.Hour)
	if err != nil {
		return err
	}
	return servicehost.Run(ctx, cfg, httpadapter.NewIdentityHandler(service))
}

// RunPlayerData（运行玩家数据服务）使用本地自动创建仓储，便于多进程联调完整登录流程。
func RunPlayerData(ctx context.Context, cfg config.ServiceConfig) error {
	// 本地装配直接复用领域层MemoryRepository；建档、角色创建、角色选择和幂等语义
	// 与生产PlayerDataService保持同一Service入口，不维护第二套旁路仓储。
	repo := playerdata.NewMemoryRepository()
	service := playerdata.NewService(repo)
	service.AttachInventory(inventory.NewService(inventory.NewMemoryRepository()))
	return servicehost.Run(ctx, cfg, httpadapter.NewPlayerDataHandler(service))
}

// RunMatch（运行比赛组织服务）使用内存Party/Ticket仓储并暴露真实内部HTTP接口。
func RunMatch(ctx context.Context, cfg config.ServiceConfig) error {
	service := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), newID)
	return servicehost.Run(ctx, cfg, httpadapter.NewMatchHandler(service))
}

// RunGameServerControl（运行游戏服务器控制服务）启用世界/竞技分配、迁移和本地Outbox Worker。
func RunGameServerControl(ctx context.Context, cfg config.ServiceConfig) error {
	internalBearerToken := strings.TrimSpace(config.Getenv("GAMESERVERCONTROL_INTERNAL_TOKEN", ""))
	if internalBearerToken == "" {
		return errors.New("GAMESERVERCONTROL_INTERNAL_TOKEN不能为空")
	}
	registry := gameserver.NewRegistry()
	secret := []byte(config.Getenv("TRANSFER_TICKET_SECRET", "dev-only-transfer-ticket-secret-32bytes-minimum"))
	transfer := servertransfer.NewServiceWithReplayStore(secret, func() time.Time { return time.Now().UTC() }, servertransfer.NewMemoryReplayStore())
	resultStore := match.NewMemoryResultStore()
	resultService := match.NewResultService(resultStore)
	service := gameservercontrol.NewService(registry, transfer, resultService, func() time.Time { return time.Now().UTC() })

	// 本地Outbox Worker使用内存仓储和日志发布器，生产环境由PostgreSQL+NATS替换。
	outboxStore := outbox.NewMemoryStore()
	dispatcher := outbox.NewDispatcherWithOptions(outboxStore, loggingPublisher{}, func() time.Time { return time.Now().UTC() }, "local-outbox", 30*time.Second, 100)
	go func() {
		_ = dispatcher.Run(ctx, 500*time.Millisecond, func(err error) { slog.Warn("本地Outbox发布失败", "error", err) })
	}()
	return servicehost.Run(ctx, cfg, httpadapter.NewGameServerControlHandler(service, internalBearerToken))
}

type loggingPublisher struct{}

func (loggingPublisher) Publish(_ context.Context, message outbox.Message) error {
	slog.Info("本地Outbox事件", "id", message.ID, "topic", message.Topic, "aggregateId", message.AggregateID)
	return nil
}

func newID(prefix string) string {
	var raw [12]byte
	if _, err := rand.Read(raw[:]); err != nil {
		panic(err)
	}
	return prefix + "-" + hex.EncodeToString(raw[:])
}
