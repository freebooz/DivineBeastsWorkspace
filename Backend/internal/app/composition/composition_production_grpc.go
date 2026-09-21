//go:build productiondeps && grpcdeps

package composition

import (
	"context"
	"log/slog"
	"net"
	"os"
	"strconv"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

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
	"divinebeasts/backend/internal/transport/grpcadapter"
	"divinebeasts/backend/internal/transport/grpcclient"
)

// RunGateway（生产gRPC模式）公网仍使用Shared OpenAPI HTTP，内部服务间调用使用gRPC。
func RunGateway(ctx context.Context, cfg config.ServiceConfig) error {
	identityConn := mustGRPCConn(requiredEnvGRPC("IDENTITY_GRPC_TARGET"))
	defer identityConn.Close()
	playerConn := mustGRPCConn(requiredEnvGRPC("PLAYER_DATA_GRPC_TARGET"))
	defer playerConn.Close()
	identityClient := grpcclient.NewIdentityClient(identityConn)
	playerClient := grpcclient.NewPlayerDataClient(playerConn)
	var party gateway.PartyPort
	var matchmaking gateway.MatchmakingPort
	if config.Getenv("ONLINE_ONLY", "false") != "true" {
		matchConn := mustGRPCConn(requiredEnvGRPC("MATCH_GRPC_TARGET"))
		defer matchConn.Close()
		client := grpcclient.NewMatchClient(matchConn)
		party = client
		matchmaking = client
	}
	handler := gateway.NewAPI(gateway.Config{ContractVersion: generatedgp.ContractVersion}, identityClient, playerClient, party, matchmaking)
	return servicehost.Run(ctx, cfg, handler)
}

func RunIdentity(ctx context.Context, cfg config.ServiceConfig) error {
	pool := mustPostgresGRPC(ctx)
	defer pool.Close()
	accessTTL, err := config.GetenvDuration("IDENTITY_ACCESS_TTL", 15*time.Minute)
	if err != nil {
		return err
	}
	refreshTTL, err := config.GetenvDuration("IDENTITY_REFRESH_TTL", 30*24*time.Hour)
	if err != nil {
		return err
	}
	service, err := identity.NewPersistentService(postgres.NewOnlineIdentityRepository(pool), identity.SystemClock{}, accessTTL, refreshTTL)
	if err != nil {
		return err
	}
	return runGRPCHost(ctx, cfg, func(server *grpc.Server) { grpcadapter.RegisterIdentityServer(server, service) })
}

func RunPlayerData(ctx context.Context, cfg config.ServiceConfig) error {
	pool := mustPostgresGRPC(ctx)
	defer pool.Close()
	service := playerdata.NewService(postgres.NewOnlinePlayerRepository(pool))
	return runGRPCHost(ctx, cfg, func(server *grpc.Server) { grpcadapter.RegisterPlayerDataServer(server, service) })
}

func RunMatch(ctx context.Context, cfg config.ServiceConfig) error {
	redisClient := mustRedisGRPC(ctx)
	defer redisClient.Close()
	partyTTL, _ := config.GetenvDuration("PARTY_TTL", 24*time.Hour)
	ticketTTL, _ := config.GetenvDuration("MATCHMAKING_TICKET_TTL", 15*time.Minute)
	service := matchapi.NewService(redisstore.NewPartyRepository(redisClient, partyTTL), redisstore.NewTicketRepository(redisClient, ticketTTL), productionIDGRPC)
	return runGRPCHost(ctx, cfg, func(server *grpc.Server) { grpcadapter.RegisterMatchAPIServer(server, service) })
}

func RunGameServerControl(ctx context.Context, cfg config.ServiceConfig) error {
	pool := mustPostgresGRPC(ctx)
	defer pool.Close()
	redisClient := mustRedisGRPC(ctx)
	defer redisClient.Close()
	publisher, err := natsjs.Open(natsjs.Config{URL: requiredEnvGRPC("NATS_URL"), ClientName: cfg.Name, ConnectTimeout: 5 * time.Second, ReconnectWait: 2 * time.Second})
	if err != nil {
		return err
	}
	defer publisher.Close()

	registry := gameserver.NewRegistry()
	worldAllocator := gameserver.NewRegistryAllocator(registry)
	agonesClient := agones.NewClient(agones.ClientConfig{BaseURL: requiredEnvGRPC("AGONES_API_URL"), BearerToken: readBearerTokenGRPC()})
	arenaAllocator := agones.NewRegistryBackedAllocator(agonesClient, registry, requiredEnvGRPC("AGONES_NAMESPACE"), requiredEnvGRPC("GAME_SERVER_BUILD_VERSION"))
	transfer := servertransfer.NewServiceWithReplayStore([]byte(requiredEnvGRPC("TRANSFER_TICKET_SECRET")), func() time.Time { return time.Now().UTC() }, redisstore.NewTransferReplayStore(redisClient))
	matchOutboxStore := postgres.NewMatchOutboxStore(pool)
	service := gameservercontrol.NewServiceWithAllocators(registry, worldAllocator, arenaAllocator, transfer, match.NewResultService(matchOutboxStore), func() time.Time { return time.Now().UTC() })

	dispatcher := outbox.NewDispatcherWithOptions(matchOutboxStore, natsjs.NewOutboxPublisher(publisher), func() time.Time { return time.Now().UTC() }, cfg.Name+":"+config.Getenv("POD_NAME", "unknown"), 30*time.Second, 100)
	go func() {
		_ = dispatcher.Run(ctx, 500*time.Millisecond, func(err error) { slog.Error("Outbox Dispatcher发布失败", "error", err) })
	}()

	return runGRPCHost(ctx, cfg, func(server *grpc.Server) {
		grpcadapter.RegisterGameServerSharedServer(server, service)
		grpcadapter.RegisterGameServerControlInternalServer(server, service)
		grpcadapter.RegisterServerTransferServer(server, service, productionIDGRPC, 30*time.Second)
		grpcadapter.RegisterMatchResultServer(server, service)
	})
}

func runGRPCHost(ctx context.Context, cfg config.ServiceConfig, register func(*grpc.Server)) error {
	listener, err := net.Listen("tcp", net.JoinHostPort(cfg.BindAddress, strconv.Itoa(cfg.GRPCPort)))
	if err != nil {
		return err
	}
	server := grpc.NewServer()
	register(server)
	grpcErr := make(chan error, 1)
	go func() {
		slog.Info("gRPC服务开始监听", "service", cfg.Name, "port", cfg.GRPCPort)
		grpcErr <- server.Serve(listener)
	}()
	httpErr := make(chan error, 1)
	go func() { httpErr <- servicehost.Run(ctx, cfg, nil) }()
	select {
	case <-ctx.Done():
		server.GracefulStop()
		return <-httpErr
	case err := <-grpcErr:
		server.Stop()
		return err
	case err := <-httpErr:
		server.GracefulStop()
		return err
	}
}

func mustGRPCConn(target string) *grpc.ClientConn {
	conn, err := grpc.NewClient(target, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		panic(err)
	}
	return conn
}

func mustPostgresGRPC(ctx context.Context) *postgres.Pool {
	pool, err := postgres.Open(ctx, postgres.Config{DSN: requiredEnvGRPC("POSTGRES_DSN"), MaxConns: 32, MinIdleConns: 4, PingTimeout: 5 * time.Second})
	if err != nil {
		panic(err)
	}
	return pool
}

func mustRedisGRPC(ctx context.Context) *redisstore.Client {
	addrs := config.GetenvCSV("REDIS_ADDRS")
	if len(addrs) == 0 {
		panic("REDIS_ADDRS不能为空")
	}
	client, err := redisstore.Open(ctx, redisstore.Config{Addrs: addrs, Password: os.Getenv("REDIS_PASSWORD"), MasterName: os.Getenv("REDIS_MASTER_NAME"), DB: 0, PoolSize: 64, MinIdleConns: 8, DialTimeout: 3 * time.Second})
	if err != nil {
		panic(err)
	}
	return client
}

func requiredEnvGRPC(key string) string {
	value := os.Getenv(key)
	if value == "" {
		panic(key + "不能为空")
	}
	return value
}

func readBearerTokenGRPC() string {
	if token := os.Getenv("AGONES_BEARER_TOKEN"); token != "" {
		return token
	}
	data, _ := os.ReadFile(config.Getenv("AGONES_BEARER_TOKEN_FILE", "/var/run/secrets/kubernetes.io/serviceaccount/token"))
	return string(data)
}

func productionIDGRPC(prefix string) string {
	return prefix + "-" + strconv.FormatInt(time.Now().UTC().UnixNano(), 36)
}
