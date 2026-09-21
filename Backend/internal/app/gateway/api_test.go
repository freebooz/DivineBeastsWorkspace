package gateway

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

type fakeIdentity struct{}

func (fakeIdentity) Login(_ context.Context, req LoginRequest) (LoginResponse, error) {
	return LoginResponse{PlayerID: "player-1", SessionID: "session-1", AccessToken: "access-1", RefreshToken: "refresh-1", ExpiresAt: "2026-09-17T02:00:00Z"}, nil
}
func (fakeIdentity) Authenticate(_ context.Context, token string) (AuthenticatedSession, error) {
	if token != "access-1" {
		return AuthenticatedSession{}, ErrUnauthorized
	}
	return AuthenticatedSession{PlayerID: "player-1", SessionID: "session-1"}, nil
}

type fakePlayerData struct{}

func (fakePlayerData) GetProfile(_ context.Context, playerID string) (PlayerProfile, error) {
	return PlayerProfile{PlayerID: playerID, GameID: "divine-beasts", DisplayName: "测试玩家", DataVersion: 1, Revision: 3}, nil
}

type fakeParty struct{}

func (fakeParty) CreateParty(_ context.Context, playerID string) (PartySnapshot, error) {
	return PartySnapshot{PartyID: "party-1", LeaderPlayerID: playerID, MemberPlayerIDs: []string{playerID}, Revision: 1}, nil
}

type fakeMatchmaking struct{}

func (fakeMatchmaking) CreateTicket(_ context.Context, playerID string, req CreateMatchmakingTicketRequest) (MatchmakingTicketResponse, error) {
	return MatchmakingTicketResponse{TicketID: "mm-1", ArenaModeID: req.ArenaModeID, PartyID: req.PartyID, PartyMemberIDs: []string{playerID}, PartySize: 1, TeamSize: 5, State: "searching"}, nil
}

func newTestAPI() http.Handler {
	return NewAPI(Config{ContractVersion: "1.0.0"}, fakeIdentity{}, fakePlayerData{}, fakeParty{}, fakeMatchmaking{})
}

// TestLoginAddsTracingHeaders（登录接口追踪头测试）验证Gateway统一注入RequestId、TraceId和契约版本。
func TestLoginAddsTracingHeaders(t *testing.T) {
	req := httptest.NewRequest(http.MethodPost, "/v1/auth/login", strings.NewReader(`{"gameId":"divine-beasts","provider":"guest","credential":"guest","clientVersion":"0.2.0","deviceId":"device-1"}`))
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("登录状态码=%d body=%s", recorder.Code, recorder.Body.String())
	}
	if recorder.Header().Get("X-Request-Id") == "" || recorder.Header().Get("X-Trace-Id") == "" {
		t.Fatal("Gateway必须返回RequestId和TraceId")
	}
	if recorder.Header().Get("X-Contract-Version") != "1.0.0" {
		t.Fatal("缺少X-Contract-Version")
	}
	var body LoginResponse
	if err := json.NewDecoder(recorder.Body).Decode(&body); err != nil {
		t.Fatal(err)
	}
	if body.PlayerID != "player-1" || body.AccessToken != "access-1" {
		t.Fatalf("登录响应错误: %+v", body)
	}
}

// TestGetProfileRequiresBearerToken（玩家资料认证测试）验证业务接口不信任客户端PlayerId，而是从Access Token解析玩家身份。
func TestGetProfileRequiresBearerToken(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/v1/player/profile", nil)
	req.Header.Set("Authorization", "Bearer access-1")
	recorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(recorder, req)
	if recorder.Code != http.StatusOK {
		t.Fatalf("状态码=%d body=%s", recorder.Code, recorder.Body.String())
	}

	req2 := httptest.NewRequest(http.MethodGet, "/v1/player/profile", nil)
	req2.Header.Set("Authorization", "Bearer invalid")
	recorder2 := httptest.NewRecorder()
	newTestAPI().ServeHTTP(recorder2, req2)
	if recorder2.Code != http.StatusUnauthorized {
		t.Fatalf("无效Token应返回401，实际=%d", recorder2.Code)
	}
}

// TestCreatePartyAndMatchmakingTicket（组队与匹配接口测试）验证Gateway只做协议适配并把PlayerID从认证上下文传给下游。
func TestCreatePartyAndMatchmakingTicket(t *testing.T) {
	partyReq := httptest.NewRequest(http.MethodPost, "/v1/party", nil)
	partyReq.Header.Set("Authorization", "Bearer access-1")
	partyRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(partyRecorder, partyReq)
	if partyRecorder.Code != http.StatusOK {
		t.Fatalf("创建Party失败: %d %s", partyRecorder.Code, partyRecorder.Body.String())
	}

	mmReq := httptest.NewRequest(http.MethodPost, "/v1/matchmaking/tickets", strings.NewReader(`{"arenaModeId":"Arena.Mode.Team5v5","partyId":"","preferredRegion":"us-west","clientRequestId":"req-1"}`))
	mmReq.Header.Set("Authorization", "Bearer access-1")
	mmReq.Header.Set("Content-Type", "application/json")
	mmRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(mmRecorder, mmReq)
	if mmRecorder.Code != http.StatusOK {
		t.Fatalf("创建匹配票据失败: %d %s", mmRecorder.Code, mmRecorder.Body.String())
	}
	var ticket MatchmakingTicketResponse
	if err := json.NewDecoder(mmRecorder.Body).Decode(&ticket); err != nil {
		t.Fatal(err)
	}
	if ticket.TeamSize != 5 || ticket.State != "searching" {
		t.Fatalf("匹配响应错误: %+v", ticket)
	}
}
