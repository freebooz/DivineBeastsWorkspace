//go:build productiondeps && !grpcdeps

package composition

import (
	"context"
	"errors"
	"log/slog"
	"os"
	"strconv"
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
	"divinebeasts/backend/internal/platform/agones"
	"divinebeasts/backend/internal/platform/cache/redisstore"
	"divinebeasts/backend/internal/platform/config"
	"divinebeasts/backend/internal/platform/database/postgres"
	"divinebeasts/backend/internal/platform/messaging/natsjs"
	"divinebeasts/backend/internal/platform/outbox"
	"divinebeasts/backend/internal/transport/httpadapter"
)

// RunGateway（生产HTTP模式）保留公网HTTP，并通过内部HTTP连接三个业务服务。
// 正式推荐使用productiondeps,grpcdeps构建，让服务间调用切换到gRPC。
func RunGateway(ctx context.Context, cfg config.ServiceConfig) error {
	identityClient := httpadapter.NewIdentityClient(httpadapter.ClientConfig{BaseURL: requiredEnv("IDENTITY_SERVICE_URL")})
	playerDataClient := httpadapter.NewPlayerDataClient(httpadapter.ClientConfig{BaseURL: requiredEnv("PLAYER_DATA_SERVICE_URL")})
	var party gateway.PartyPort
	var matchmaking gateway.MatchmakingPort
	if config.Getenv("ONLINE_ONLY","false")!="true" {client:=httpadapter.NewMatchClient(httpadapter.ClientConfig{BaseURL:requiredEnv("MATCH_SERVICE_URL")});party=client;matchmaking=client}
	handler := gateway.NewAPI(gateway.Config{ContractVersion: generatedgp.ContractVersion}, identityClient, playerDataClient, party, matchmaking)
	return servicehost.Run(ctx, cfg, handler)
}

func RunIdentity(ctx context.Context, cfg config.ServiceConfig) error {
	pool:=mustPostgres(ctx)
	defer pool.Close()
	accessTTL,err:=config.GetenvDuration("IDENTITY_ACCESS_TTL",15*time.Minute);if err!=nil{return err}
	refreshTTL,err:=config.GetenvDuration("IDENTITY_REFRESH_TTL",30*24*time.Hour);if err!=nil{return err}
	service,err:=identity.NewPersistentService(postgres.NewOnlineIdentityRepository(pool),identity.SystemClock{},accessTTL,refreshTTL);if err!=nil{return err}
	return servicehost.Run(ctx, cfg, httpadapter.NewIdentityHandler(service))
}

func RunPlayerData(ctx context.Context, cfg config.ServiceConfig) error {
	pool := mustPostgres(ctx)
	defer pool.Close()
	service := playerdata.NewService(postgres.NewOnlinePlayerRepository(pool))
	return servicehost.Run(ctx, cfg, httpadapter.NewPlayerDataHandler(service))
}

func RunMatch(ctx context.Context, cfg config.ServiceConfig) error {
	redisClient := mustRedis(ctx)
	defer redisClient.Close()
	partyTTL, _ := config.GetenvDuration("PARTY_TTL", 24*time.Hour)
	ticketTTL, _ := config.GetenvDuration("MATCHMAKING_TICKET_TTL", 15*time.Minute)
	service := matchapi.NewService(redisstore.NewPartyRepository(redisClient, partyTTL), redisstore.NewTicketRepository(redisClient, ticketTTL), productionID)
	return servicehost.Run(ctx, cfg, httpadapter.NewMatchHandler(service))
}

func RunGameServerControl(ctx context.Context, cfg config.ServiceConfig) error {
	pool := mustPostgres(ctx)
	defer pool.Close()
	redisClient := mustRedis(ctx)
	defer redisClient.Close()
	publisher, err := natsjs.Open(natsjs.Config{URL: requiredEnv("NATS_URL"), ClientName: cfg.Name, ConnectTimeout: 5 * time.Second, ReconnectWait: 2 * time.Second})
	if err != nil {
		return err
	}
	defer publisher.Close()

	registry := gameserver.NewRegistry()
	worldAllocator := gameserver.NewRegistryAllocator(registry)
	agonesClient := agones.NewClient(agones.ClientConfig{BaseURL: requiredEnv("AGONES_API_URL"), BearerToken: readBearerToken()})
	arenaAllocator := agones.NewRegistryBackedAllocator(agonesClient, registry, requiredEnv("AGONES_NAMESPACE"), requiredEnv("GAME_SERVER_BUILD_VERSION"))

	secret := []byte(requiredEnv("TRANSFER_TICKET_SECRET"))
	transfer := servertransfer.NewServiceWithReplayStore(secret, func() time.Time { return time.Now().UTC() }, redisstore.NewTransferReplayStore(redisClient))
	matchOutboxStore := postgres.NewMatchOutboxStore(pool)
	resultService := match.NewResultService(matchOutboxStore)
	service := gameservercontrol.NewServiceWithAllocators(registry, worldAllocator, arenaAllocator, transfer, resultService, func() time.Time { return time.Now().UTC() })

	dispatcher := outbox.NewDispatcherWithOptions(matchOutboxStore, natsjs.NewOutboxPublisher(publisher), func() time.Time { return time.Now().UTC() }, cfg.Name+":"+config.Getenv("POD_NAME", "local"), 30*time.Second, 100)
	go func() {
		_ = dispatcher.Run(ctx, 500*time.Millisecond, func(err error) { slog.Error("Outbox Dispatcher发布失败", "error", err) })
	}()
	return servicehost.Run(ctx, cfg, httpadapter.NewGameServerControlHandler(service))
}

func mustPostgres(ctx context.Context) *postgres.Pool {
	maxConns := int32(parseInt(config.Getenv("POSTGRES_MAX_CONNS", "32"), 32))
	pool, err := postgres.Open(ctx, postgres.Config{DSN: requiredEnv("POSTGRES_DSN"), MaxConns: maxConns, MinIdleConns: 4, PingTimeout: 5 * time.Second})
	if err != nil {
		panic(err)
	}
	return pool
}

func mustRedis(ctx context.Context) *redisstore.Client {
	addrs := config.GetenvCSV("REDIS_ADDRS")
	if len(addrs) == 0 {
		panic("REDIS_ADDRS不能为空")
	}
	client, err := redisstore.Open(ctx, redisstore.Config{Addrs: addrs, Password: os.Getenv("REDIS_PASSWORD"), MasterName: os.Getenv("REDIS_MASTER_NAME"), DB: parseInt(config.Getenv("REDIS_DB", "0"), 0), PoolSize: parseInt(config.Getenv("REDIS_POOL_SIZE", "64"), 64), MinIdleConns: 8, DialTimeout: 3 * time.Second})
	if err != nil {
		panic(err)
	}
	return client
}

func requiredEnv(key string) string {
	value := os.Getenv(key)
	if value == "" {
		panic(key + "不能为空")
	}
	return value
}

func parseInt(text string, fallback int) int {
	value, err := strconv.Atoi(text)
	if err != nil {
		return fallback
	}
	return value
}

func readBearerToken() string {
	if token := os.Getenv("AGONES_BEARER_TOKEN"); token != "" {
		return token
	}
	path := config.Getenv("AGONES_BEARER_TOKEN_FILE", "/var/run/secrets/kubernetes.io/serviceaccount/token")
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(data)
}

func productionID(prefix string) string {
	return prefix + "-" + strconv.FormatInt(time.Now().UTC().UnixNano(), 36)
}

var _ = errors.New // 保持错误包在后续生产配置扩展时可直接使用。
