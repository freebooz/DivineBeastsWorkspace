package httpadapter

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
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

// TestGameServerControlHTTPWorldTransferRoundTrip（世界跨服HTTP联调测试）验证OpenWorld.Hub分配、Assignment绑定、票据消费和重放拒绝均经过真实HTTP Handler。
func TestGameServerControlHTTPWorldTransferRoundTrip(t *testing.T) {
	now := time.Date(2026, 9, 21, 7, 0, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	transferService := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	service := gameservercontrol.NewService(registry, transferService, match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	server := httptest.NewServer(NewGameServerControlHandler(service))
	defer server.Close()

	postJSON(t, server.URL+"/internal/v1/gameservers/register", gameservercontrol.RegisterInput{
		GameID: "divine-beasts", GameServerID: "ow-hub-http-1", ServerRoleID: gameservercontract.RoleOpenWorld,
		ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", WorldID: "World.OpenWorld.Hub",
		PublicEndpoint: "127.0.0.1:7777", BuildVersion: "1.1.0", ProtocolVersion: 1, Capacity: 100,
	}, http.StatusOK, nil)
	postJSON(t, server.URL+"/internal/v1/gameservers/ready", map[string]any{"gameServerId": "ow-hub-http-1"}, http.StatusOK, nil)

	var worldTransfer gameservercontrol.WorldTransferResult
	postJSON(t, server.URL+"/internal/v1/gameservers/allocate-world-transfer", map[string]any{
		"world": map[string]any{
			"ExperienceID": gameservercontract.ExperienceOpenWorldHub,
			"WorldID":      "World.OpenWorld.Hub",
			"RegionID":     "us-west",
			"PlayerSlots":  1,
		},
		"ticketId": "ticket-http-1", "gameId": "divine-beasts", "playerId": "player-http-1", "sessionId": "session-http-1", "ttlMilliseconds": 30000,
	}, http.StatusOK, &worldTransfer)
	if worldTransfer.Assignment.ServerRoleID != gameservercontract.RoleOpenWorld || worldTransfer.Ticket.DestinationExperienceID != gameservercontract.ExperienceOpenWorldHub {
		t.Fatalf("HTTP世界跨服结果错误: %+v", worldTransfer)
	}

	validateRequest := map[string]any{"ticket": worldTransfer.Ticket, "destinationGameServerId": "ow-hub-http-1"}
	var validation servertransfer.ValidationResult
	postJSON(t, server.URL+"/internal/v1/gameservers/validate-transfer", validateRequest, http.StatusOK, &validation)
	if validation.AssignmentID != worldTransfer.Assignment.AssignmentID {
		t.Fatalf("HTTP迁移验证Assignment不一致: %+v", validation)
	}
	postJSON(t, server.URL+"/internal/v1/gameservers/validate-transfer", validateRequest, http.StatusUnauthorized, nil)
}

func postJSON(t *testing.T, url string, input any, expectedStatus int, output any) {
	t.Helper()
	body, err := json.Marshal(input)
	if err != nil {
		t.Fatal(err)
	}
	response, err := http.Post(url, "application/json", bytes.NewReader(body))
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
