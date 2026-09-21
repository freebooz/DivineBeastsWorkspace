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
	"sync"
	"time"

	generatedgp "divinebeasts/backend/generated/gameplatform"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/app/servicehost"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/identity"
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
	handler := gateway.NewAPI(gateway.Config{ContractVersion: generatedgp.ContractVersion}, identityClient, playerDataClient, matchClient, matchClient)
	return servicehost.Run(ctx, cfg, handler)
}

// RunIdentity（运行身份服务）使用内存SessionRepository并暴露真实内部HTTP接口。
func RunIdentity(ctx context.Context, cfg config.ServiceConfig) error {
	service := identity.NewService(identity.NewMemorySessionRepository(), identity.SystemClock{}, identity.CryptoTokenGenerator{}, 15*time.Minute, 30*24*time.Hour)
	return servicehost.Run(ctx, cfg, httpadapter.NewIdentityHandler(service))
}

// RunPlayerData（运行玩家数据服务）使用本地自动创建仓储，便于多进程联调完整登录流程。
func RunPlayerData(ctx context.Context, cfg config.ServiceConfig) error {
	repo := newAutoCreatePlayerRepository()
	service := playerdata.NewService(repo)
	return servicehost.Run(ctx, cfg, httpadapter.NewPlayerDataHandler(service))
}

// RunMatch（运行比赛组织服务）使用内存Party/Ticket仓储并暴露真实内部HTTP接口。
func RunMatch(ctx context.Context, cfg config.ServiceConfig) error {
	service := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), newID)
	return servicehost.Run(ctx, cfg, httpadapter.NewMatchHandler(service))
}

// RunGameServerControl（运行游戏服务器控制服务）启用世界/竞技分配、迁移和本地Outbox Worker。
func RunGameServerControl(ctx context.Context, cfg config.ServiceConfig) error {
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
	return servicehost.Run(ctx, cfg, httpadapter.NewGameServerControlHandler(service))
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

// autoCreatePlayerRepository（本地自动玩家仓储）在首次读取登录产生的PlayerID时创建默认资料。
type autoCreatePlayerRepository struct {
	mu    sync.RWMutex
	items map[string]playerdata.Profile
}

func newAutoCreatePlayerRepository() *autoCreatePlayerRepository {
	return &autoCreatePlayerRepository{items: map[string]playerdata.Profile{}}
}

func (r *autoCreatePlayerRepository) Get(_ context.Context, playerID string) (playerdata.Profile, error) {
	if playerID == "" {
		return playerdata.Profile{}, errors.New("PlayerID不能为空")
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	value, ok := r.items[playerID]
	if !ok {
		value = playerdata.Profile{PlayerID: playerID, GameID: "divine-beasts", DisplayName: "新玩家", DataVersion: 1, Revision: 1, DefaultWorldID: "World.OpenWorld.Hub"}
		r.items[playerID] = value
	}
	value.OwnedCharacterIDs = append([]string(nil), value.OwnedCharacterIDs...)
	return value, nil
}

func (r *autoCreatePlayerRepository) Save(_ context.Context, profile playerdata.Profile, expectedRevision int64) (playerdata.Profile, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	current, ok := r.items[profile.PlayerID]
	if !ok || current.Revision != expectedRevision {
		return playerdata.Profile{}, errors.New("PLAYER_DATA_CONFLICT: 玩家资料Revision冲突")
	}
	profile.Revision = expectedRevision + 1
	r.items[profile.PlayerID] = profile
	return profile, nil
}
