package httpadapter

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchapi"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/identity"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/modules/servertransfer"
)

const gameServerControlTestToken = "test-internal-token"

// TestInternalHTTPTransportRoundTrip（内部HTTP传输联调测试）验证Gateway侧HTTP Client确实通过网络调用三个下游服务。
func TestInternalHTTPTransportRoundTrip(t *testing.T) {
	ctx := context.Background()

	identityService := identity.NewService(identity.NewMemorySessionRepository(), fixedClock{value: time.Date(2026, 9, 21, 6, 0, 0, 0, time.UTC)}, deterministicTokenGenerator{}, 15*time.Minute, 24*time.Hour)
	identityServer := httptest.NewServer(NewIdentityHandler(identityService))
	defer identityServer.Close()
	identityClient := NewIdentityClient(ClientConfig{BaseURL: identityServer.URL})
	login, err := identityClient.Login(ctx, gateway.LoginRequest{GameID: "divine-beasts", Provider: "guest", Credential: "guest", DeviceID: "device-1"})
	if err != nil {
		t.Fatalf("Identity HTTP登录失败: %v", err)
	}
	authenticated, err := identityClient.Authenticate(ctx, login.AccessToken)
	if err != nil || authenticated.PlayerID != login.PlayerID {
		t.Fatalf("Identity HTTP认证失败: session=%+v err=%v", authenticated, err)
	}

	playerRepo := playerdata.NewMemoryRepository()
	playerRepo.Seed(playerdata.Profile{PlayerID: login.PlayerID, GameID: "divine-beasts", DisplayName: "测试玩家", DataVersion: 1, Revision: 1, DefaultWorldID: "World.OpenWorld.Hub"})
	playerServer := httptest.NewServer(NewPlayerDataHandler(playerdata.NewService(playerRepo)))
	defer playerServer.Close()
	playerClient := NewPlayerDataClient(ClientConfig{BaseURL: playerServer.URL})
	profile, err := playerClient.GetProfile(ctx, login.PlayerID)
	if err != nil || profile.DefaultWorldID != "World.OpenWorld.Hub" {
		t.Fatalf("PlayerData HTTP读取失败: profile=%+v err=%v", profile, err)
	}

	matchService := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(prefix string) string { return prefix + "-fixed" })
	matchServer := httptest.NewServer(NewMatchHandler(matchService))
	defer matchServer.Close()
	matchClient := NewMatchClient(ClientConfig{BaseURL: matchServer.URL})
	party, err := matchClient.CreateParty(ctx, login.PlayerID)
	if err != nil {
		t.Fatalf("Match HTTP创建Party失败: %v", err)
	}
	ticket, err := matchClient.CreateTicket(ctx, login.PlayerID, gateway.CreateMatchmakingTicketRequest{ArenaModeID: "Arena.Mode.Duel1v1", PartyID: party.PartyID, PreferredRegion: "us-west", ClientRequestID: "request-1"})
	if err != nil || ticket.TeamSize != 1 || len(ticket.PartyMemberIDs) != 1 {
		t.Fatalf("Match HTTP创建Ticket失败: ticket=%+v err=%v", ticket, err)
	}
}

// TestGameServerControlHTTPWorldTransferRoundTrip（大厅跨服HTTP联调测试）验证OpenWorld.Hub分配、Assignment绑定、票据消费和重放拒绝均经过真实HTTP Handler。
func TestGameServerControlHTTPWorldTransferRoundTrip(t *testing.T) {
	// 内部世界分配接口不得匿名调用；Gateway或Backend内部服务必须携带内部Bearer。
	unauthorizedRequest := httptest.NewRequest(http.MethodPost, "/internal/v1/gameservers/allocate-world-transfer", strings.NewReader(`{}`))
	unauthorizedRequest.Header.Set("Content-Type", "application/json")
	unauthorizedRecorder := httptest.NewRecorder()
	NewGameServerControlHandler(
		gameservercontrol.NewService(
			gameserver.NewRegistry(),
			servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now),
			match.NewResultService(match.NewMemoryResultStore()),
			time.Now,
		),
		gameServerControlTestToken,
	).ServeHTTP(unauthorizedRecorder, unauthorizedRequest)
	if unauthorizedRecorder.Code != http.StatusUnauthorized {
		t.Fatalf("匿名世界分配必须返回401，实际=%d", unauthorizedRecorder.Code)
	}
	now := time.Date(2026, 9, 21, 7, 0, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	transferService := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	service := gameservercontrol.NewService(registry, transferService, match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	server := httptest.NewServer(NewGameServerControlHandler(service, gameServerControlTestToken))
	defer server.Close()

	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/register", gameservercontrol.RegisterInput{
		GameID: "divine-beasts", GameServerID: "openworld-hub-http-1", ServerBootID: "boot-openworld-hub-http-1", ServerRoleID: gameservercontract.RoleOpenWorld,
		ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", WorldID: "World.OpenWorld.Hub",
		PublicEndpoint: "127.0.0.1:7777", BuildVersion: "1.1.0", ProtocolVersion: 1, Capacity: 100,
	}, map[string]string{"Authorization": "Bearer " + gameServerControlTestToken, "X-Game-Server-Id": "openworld-hub-http-1", "X-Game-Server-Boot-Id": "boot-openworld-hub-http-1"}, http.StatusOK, nil)
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/heartbeat", map[string]any{
		"gameServerId": "openworld-hub-http-1", "currentPlayers": 0, "status": gameserver.StatusStarting,
	}, map[string]string{"Authorization": "Bearer " + gameServerControlTestToken, "X-Game-Server-Id": "openworld-hub-http-1", "X-Game-Server-Boot-Id": "boot-openworld-hub-http-1"}, http.StatusOK, nil)
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/ready", map[string]any{"gameServerId": "openworld-hub-http-1"}, map[string]string{"Authorization": "Bearer " + gameServerControlTestToken, "X-Game-Server-Id": "openworld-hub-http-1", "X-Game-Server-Boot-Id": "boot-openworld-hub-http-1"}, http.StatusOK, nil)

	// Gateway内部客户端必须通过受保护的同一GameServerControl链路完成世界分配与签票，
	// 验证其JSON适配与公共WorldEntry响应字段真实可用，而不是只测Handler直调。
	worldEntryClient := NewGameServerControlClient(GameServerControlClientConfig{
		ClientConfig:  ClientConfig{BaseURL: server.URL},
		BearerToken:   gameServerControlTestToken,
		DefaultRegion: "us-west",
	})
	worldEntry, err := worldEntryClient.AllocateWorldEntry(context.Background(), gateway.WorldEntryAllocationRequest{
		RequestID:           "ticket-client-http-1",
		GameID:              "divine-beasts",
		PlayerID:            "player-http-client-1",
		SessionID:           "session-http-client-1",
		CharacterID:         "character-http-client-1",
		DesiredExperienceID: gameservercontract.ExperienceOpenWorldHub,
	})
	if err != nil {
		t.Fatalf("GameServerControl内部客户端世界进入失败: %v", err)
	}
	if worldEntry.GameServerID != "openworld-hub-http-1" ||
		worldEntry.CharacterID != "character-http-client-1" ||
		worldEntry.SessionID != "session-http-client-1" ||
		worldEntry.GameSessionID == "" ||
		worldEntry.ServerBootID != "boot-openworld-hub-http-1" ||
		worldEntry.ProtocolVersion != 1 || worldEntry.SessionEpoch == 0 ||
		worldEntry.Endpoint != "127.0.0.1:7777" ||
		worldEntry.TransferTicket == "" {
		t.Fatalf("Gateway世界进入适配结果错误: %+v", worldEntry)
	}
	var adaptedTicket servertransfer.Ticket
	if err := json.Unmarshal([]byte(worldEntry.TransferTicket), &adaptedTicket); err != nil ||
		adaptedTicket.TicketID != worldEntry.TicketID ||
		adaptedTicket.AssignmentID != worldEntry.AssignmentID {
		t.Fatalf("世界进入TransferTicket序列化错误: ticket=%+v err=%v", adaptedTicket, err)
	}

	var worldTransfer gameservercontrol.WorldTransferResult
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/allocate-world-transfer", map[string]any{
		"world": map[string]any{
			"ExperienceID": gameservercontract.ExperienceOpenWorldHub,
			"WorldID":      "World.OpenWorld.Hub",
			"RegionID":     "us-west",
			"PlayerSlots":  1,
		},
		"ticketId": "ticket-http-1", "gameId": "divine-beasts", "playerId": "player-http-1", "sessionId": "session-http-1", "ttlMilliseconds": 30000,
	}, map[string]string{"Authorization": "Bearer " + gameServerControlTestToken}, http.StatusOK, &worldTransfer)
	if worldTransfer.Assignment.ServerRoleID != gameservercontract.RoleOpenWorld || worldTransfer.Ticket.DestinationExperienceID != gameservercontract.ExperienceOpenWorldHub {
		t.Fatalf("HTTP世界跨服结果错误: %+v", worldTransfer)
	}

	validateRequest := map[string]any{"ticket": worldTransfer.Ticket, "destinationGameServerId": "openworld-hub-http-1"}
	var validation servertransfer.ValidationResult
	gameServerHeaders := map[string]string{
		"Authorization":         "Bearer " + gameServerControlTestToken,
		"X-Game-Server-Id":      "openworld-hub-http-1",
		"X-Game-Server-Boot-Id": "boot-openworld-hub-http-1",
	}
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/validate-transfer", validateRequest, gameServerHeaders, http.StatusOK, &validation)
	if validation.AssignmentID != worldTransfer.Assignment.AssignmentID {
		t.Fatalf("HTTP迁移验证Assignment不一致: %+v", validation)
	}
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/validate-transfer", validateRequest, gameServerHeaders, http.StatusUnauthorized, nil)
	postJSONWithHeaders(t, server.URL+"/internal/v1/gameservers/drain", map[string]any{"gameServerId": "openworld-hub-http-1"}, map[string]string{"Authorization": "Bearer " + gameServerControlTestToken, "X-Game-Server-Id": "openworld-hub-http-1", "X-Game-Server-Boot-Id": "boot-openworld-hub-http-1"}, http.StatusOK, nil)
}

// TestGameServerControlLifecycleRequiresAuthentication（服务器生命周期接口拒绝未认证请求）防止未认证调用注册、更新或排空服务器实例。
func TestGameServerControlLifecycleRequiresAuthentication(t *testing.T) {
	now := time.Date(2026, 9, 21, 7, 0, 0, 0, time.UTC)
	newService := func() *gameservercontrol.Service {
		registry := gameserver.NewRegistry()
		transfer := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
		return gameservercontrol.NewService(registry, transfer, match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	}
	registration := gameservercontrol.RegisterInput{
		GameID: "divine-beasts", GameServerID: "lifecycle-http-1", ServerBootID: "boot-lifecycle-http-1", ServerRoleID: gameservercontract.RoleOpenWorld,
		ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", WorldID: "World.OpenWorld.Hub",
		PublicEndpoint: "127.0.0.1:7777", BuildVersion: "1.1.0", ProtocolVersion: 1, Capacity: 100,
	}
	tests := []struct {
		name    string
		path    string
		body    any
		prepare func(*gameservercontrol.Service) error
	}{
		{name: "register", path: "/internal/v1/gameservers/register", body: registration},
		{
			name: "heartbeat", path: "/internal/v1/gameservers/heartbeat",
			body:    map[string]any{"gameServerId": registration.GameServerID, "currentPlayers": 1, "status": gameserver.StatusStarting},
			prepare: func(service *gameservercontrol.Service) error { return service.Register(registration) },
		},
		{
			name: "ready", path: "/internal/v1/gameservers/ready",
			body:    map[string]any{"gameServerId": registration.GameServerID},
			prepare: func(service *gameservercontrol.Service) error { return service.Register(registration) },
		},
		{
			name: "drain", path: "/internal/v1/gameservers/drain",
			body: map[string]any{"gameServerId": registration.GameServerID},
			prepare: func(service *gameservercontrol.Service) error {
				if err := service.Register(registration); err != nil {
					return err
				}
				return service.SetReady(registration.GameServerID)
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			service := newService()
			if test.prepare != nil {
				if err := test.prepare(service); err != nil {
					t.Fatalf("准备真实服务器实例失败: %v", err)
				}
			}
			handler := NewGameServerControlHandler(service, gameServerControlTestToken)
			for _, credentials := range []struct {
				name  string
				token string
			}{{name: "missing"}, {name: "wrong", token: "Bearer wrong-token"}} {
				t.Run(credentials.name, func(t *testing.T) {
					body, err := json.Marshal(test.body)
					if err != nil {
						t.Fatalf("序列化请求失败: %v", err)
					}
					request := httptest.NewRequest(http.MethodPost, test.path, bytes.NewReader(body))
					request.Header.Set("Content-Type", "application/json")
					if credentials.token != "" {
						request.Header.Set("Authorization", credentials.token)
					}
					response := httptest.NewRecorder()
					handler.ServeHTTP(response, request)
					if response.Code != http.StatusUnauthorized {
						t.Fatalf("未认证的 %s 请求状态码=%d，期望=%d，响应=%s", test.path, response.Code, http.StatusUnauthorized, response.Body.String())
					}
				})
			}
		})
	}
}

// TestGameServerControlLifecycleBindsHeaderToRequestBody（服务器生命周期接口校验实例身份一致性）防止授权请求用请求头和请求体操作不同服务器。
func TestGameServerControlLifecycleBindsHeaderToRequestBody(t *testing.T) {
	now := time.Date(2026, 9, 21, 7, 0, 0, 0, time.UTC)
	registration := gameservercontrol.RegisterInput{
		GameID: "divine-beasts", GameServerID: "lifecycle-http-1", ServerBootID: "boot-lifecycle-http-1", ServerRoleID: gameservercontract.RoleOpenWorld,
		ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", WorldID: "World.OpenWorld.Hub",
		PublicEndpoint: "127.0.0.1:7777", BuildVersion: "1.1.0", ProtocolVersion: 1, Capacity: 100,
	}
	tests := []struct {
		name    string
		path    string
		body    any
		prepare func(*gameservercontrol.Service) error
	}{
		{name: "register", path: "/internal/v1/gameservers/register", body: registration},
		{
			name: "heartbeat", path: "/internal/v1/gameservers/heartbeat",
			body:    map[string]any{"gameServerId": registration.GameServerID, "currentPlayers": 1, "status": gameserver.StatusStarting},
			prepare: func(service *gameservercontrol.Service) error { return service.Register(registration) },
		},
		{
			name: "ready", path: "/internal/v1/gameservers/ready",
			body:    map[string]any{"gameServerId": registration.GameServerID},
			prepare: func(service *gameservercontrol.Service) error { return service.Register(registration) },
		},
		{
			name: "drain", path: "/internal/v1/gameservers/drain",
			body: map[string]any{"gameServerId": registration.GameServerID},
			prepare: func(service *gameservercontrol.Service) error {
				if err := service.Register(registration); err != nil {
					return err
				}
				return service.SetReady(registration.GameServerID)
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			registry := gameserver.NewRegistry()
			transfer := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
			service := gameservercontrol.NewService(registry, transfer, match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
			if test.prepare != nil {
				if err := test.prepare(service); err != nil {
					t.Fatalf("准备真实服务器实例失败: %v", err)
				}
			}
			body, err := json.Marshal(test.body)
			if err != nil {
				t.Fatalf("序列化请求失败: %v", err)
			}
			request := httptest.NewRequest(http.MethodPost, test.path, bytes.NewReader(body))
			request.Header.Set("Authorization", "Bearer "+gameServerControlTestToken)
			request.Header.Set("X-Game-Server-Id", "different-server")
			request.Header.Set("X-Game-Server-Boot-Id", registration.ServerBootID)
			request.Header.Set("Content-Type", "application/json")
			response := httptest.NewRecorder()
			NewGameServerControlHandler(service, gameServerControlTestToken).ServeHTTP(response, request)
			if response.Code != http.StatusForbidden {
				t.Fatalf("身份头与请求体不一致的 %s 请求状态码=%d，期望=%d，响应=%s", test.path, response.Code, http.StatusForbidden, response.Body.String())
			}
		})
	}
}

func postJSON(t *testing.T, url string, input any, expectedStatus int, output any) {
	postJSONWithHeaders(t, url, input, nil, expectedStatus, output)
}

func postJSONWithHeaders(t *testing.T, url string, input any, headers map[string]string, expectedStatus int, output any) {
	t.Helper()
	body, err := json.Marshal(input)
	if err != nil {
		t.Fatal(err)
	}
	request, err := http.NewRequest(http.MethodPost, url, bytes.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	request.Header.Set("Content-Type", "application/json")
	for name, value := range headers {
		request.Header.Set(name, value)
	}
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	if response.StatusCode != expectedStatus {
		t.Fatalf("POST %s状态码=%d，期望=%d", url, response.StatusCode, expectedStatus)
	}
	if output != nil && expectedStatus >= 200 && expectedStatus < 300 {
		if err := json.NewDecoder(response.Body).Decode(output); err != nil {
			t.Fatal(err)
		}
	}
}

type fixedClock struct{ value time.Time }

func (c fixedClock) Now() time.Time { return c.value }

type deterministicTokenGenerator struct{ counter int }

func (deterministicTokenGenerator) NewToken(prefix string) string { return prefix + "-fixed" }
