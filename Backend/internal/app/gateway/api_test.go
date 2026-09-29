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
	return PlayerProfile{PlayerID: playerID, GameID: "divine-beasts", DisplayName: "测试玩家", DataVersion: 1, Revision: 3, SelectedCharacterID: "character-1", OwnedCharacterIDs: []string{"character-1"}}, nil
}
func (fakePlayerData) EnsureProfile(_ context.Context, _, _ string) error { return nil }
func (fakePlayerData) ListCharacters(_ context.Context, playerID string) ([]CharacterSummary, error) {
	return []CharacterSummary{{CharacterID: "character-1", HeroDefinitionID: "Hero.Zodiac.Tiger", CharacterName: "白君", CharacterRevision: 1, OnboardingState: "TutorialRequired", Status: "Active"}}, nil
}
func (fakePlayerData) CreateCharacter(_ context.Context, playerID string, req CreateCharacterRequest) (CharacterSummary, error) {
	return CharacterSummary{CharacterID: "character-created", HeroDefinitionID: req.HeroDefinitionID, CharacterName: req.CharacterName, CharacterRevision: 1, OnboardingState: "TutorialRequired", Status: "Active"}, nil
}
func (fakePlayerData) SelectCharacter(_ context.Context, playerID string, req CharacterSelectionRequest) (CharacterSelectionResponse, error) {
	return CharacterSelectionResponse{SelectionRequestID: req.SelectionRequestID, ProfileRevision: 4, Character: CharacterSummary{CharacterID: req.CharacterID, HeroDefinitionID: "Hero.Zodiac.Tiger", CharacterName: "白君", CharacterRevision: req.ExpectedCharacterRevision, OnboardingState: "TutorialRequired", Status: "Active"}}, nil
}

type fakeParty struct{}

func (fakeParty) CreateParty(_ context.Context, playerID string) (PartySnapshot, error) {
	return PartySnapshot{PartyID: "party-1", LeaderPlayerID: playerID, MemberPlayerIDs: []string{playerID}, Revision: 1}, nil
}

type fakeMatchmaking struct{}

type fakeWorldEntry struct{}

func (fakeWorldEntry) AllocateWorldEntry(_ context.Context, req WorldEntryAllocationRequest) (WorldEntryResponse, error) {
	return WorldEntryResponse{
		AssignmentID:    "world:openworld-1:" + req.DesiredExperienceID,
		GameServerID:    "openworld-1",
		ServerRoleID:    "GameServer.Role.OpenWorld",
		ExperienceID:    req.DesiredExperienceID,
		WorldID:         "World.OpenWorld.Hub",
		MapID:           "World.OpenWorld.Hub",
		RegionID:        "us-west",
		TicketID:        req.RequestID,
		CharacterID:     req.CharacterID,
		SessionID:       req.SessionID,
		GameSessionID:   "game-session-test-1",
		ServerBootID:    "boot-openworld-1",
		ProtocolVersion: 1,
		SessionEpoch:    3,
		Endpoint:        "127.0.0.1:7777",
		TransferTicket:  `{"ticketId":"entry-1","signature":"redacted-test"}`,
	}, nil
}

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

// TestCharacterRoutesUseAuthenticatedSubject（角色接口认证主体测试）验证客户端无法通过请求体越权声明其他玩家。
func TestCharacterRoutesUseAuthenticatedSubject(t *testing.T) {
	listReq := httptest.NewRequest(http.MethodGet, "/v1/player/characters", nil)
	listReq.Header.Set("Authorization", "Bearer access-1")
	listRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(listRecorder, listReq)
	if listRecorder.Code != http.StatusOK {
		t.Fatalf("角色列表失败: %d %s", listRecorder.Code, listRecorder.Body.String())
	}
	var list []CharacterSummary
	if err := json.NewDecoder(listRecorder.Body).Decode(&list); err != nil || len(list) != 1 || list[0].CharacterID != "character-1" {
		t.Fatalf("角色列表响应错误: %+v %v", list, err)
	}

	createReq := httptest.NewRequest(http.MethodPost, "/v1/player/characters", strings.NewReader(`{"creationRequestId":"create-1","heroDefinitionId":"Hero.Zodiac.Dragon","characterName":"苍龙"}`))
	createReq.Header.Set("Authorization", "Bearer access-1")
	createReq.Header.Set("Content-Type", "application/json")
	createRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(createRecorder, createReq)
	if createRecorder.Code != http.StatusOK {
		t.Fatalf("创建角色失败: %d %s", createRecorder.Code, createRecorder.Body.String())
	}

	// 公网契约没有playerId字段；试图注入其他玩家身份必须按未知字段拒绝。
	forged := httptest.NewRequest(http.MethodPost, "/v1/player/character-selection", strings.NewReader(`{"playerId":"other-player","selectionRequestId":"select-1","characterId":"character-1","expectedCharacterRevision":1}`))
	forged.Header.Set("Authorization", "Bearer access-1")
	forged.Header.Set("Content-Type", "application/json")
	forgedRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(forgedRecorder, forged)
	if forgedRecorder.Code != http.StatusBadRequest {
		t.Fatalf("越权playerId字段必须被拒绝，实际=%d body=%s", forgedRecorder.Code, forgedRecorder.Body.String())
	}

	selectReq := httptest.NewRequest(http.MethodPost, "/v1/player/character-selection", strings.NewReader(`{"selectionRequestId":"select-1","characterId":"character-1","expectedCharacterRevision":1}`))
	selectReq.Header.Set("Authorization", "Bearer access-1")
	selectReq.Header.Set("Content-Type", "application/json")
	selectRecorder := httptest.NewRecorder()
	newTestAPI().ServeHTTP(selectRecorder, selectReq)
	if selectRecorder.Code != http.StatusOK {
		t.Fatalf("角色选择失败: %d %s", selectRecorder.Code, selectRecorder.Body.String())
	}
}

// TestWorldEntryUsesAuthenticatedSelection（世界进入认证与角色一致性测试）验证客户端不能伪造Player/Session，且只能使用服务端当前已选角色。
func TestWorldEntryUsesAuthenticatedSelection(t *testing.T) {
	handler := NewAPI(
		Config{ContractVersion: "1.0.0"},
		fakeIdentity{},
		fakePlayerData{},
		fakeParty{},
		fakeMatchmaking{},
		fakeWorldEntry{},
	)
	valid := httptest.NewRequest(
		http.MethodPost,
		"/v1/divinebeasts/world-entry",
		strings.NewReader(`{"requestId":"entry-1","characterId":"character-1","desiredExperienceId":"Experience.OpenWorld.Hub","expectedCharacterRevision":1,"preferredRegion":"us-west"}`),
	)
	valid.Header.Set("Authorization", "Bearer access-1")
	valid.Header.Set("Content-Type", "application/json")
	validRecorder := httptest.NewRecorder()
	handler.ServeHTTP(validRecorder, valid)
	if validRecorder.Code != http.StatusOK {
		t.Fatalf("世界进入失败: %d %s", validRecorder.Code, validRecorder.Body.String())
	}
	if validRecorder.Header().Get("Cache-Control") != "no-store" {
		t.Fatal("世界进入响应必须禁止缓存一次性迁移材料")
	}
	var response WorldEntryResponse
	if err := json.NewDecoder(validRecorder.Body).Decode(&response); err != nil {
		t.Fatal(err)
	}
	if response.CharacterID != "character-1" || response.SessionID != "session-1" ||
		response.GameSessionID == "" || response.ServerBootID == "" ||
		response.ProtocolVersion == 0 || response.SessionEpoch == 0 ||
		response.TransferTicket == "" || response.Endpoint == "" {
		t.Fatalf("世界进入响应错误: %+v", response)
	}

	forged := httptest.NewRequest(
		http.MethodPost,
		"/v1/divinebeasts/world-entry",
		strings.NewReader(`{"requestId":"entry-2","characterId":"character-other","desiredExperienceId":"Experience.OpenWorld.Hub","expectedCharacterRevision":1}`),
	)
	forged.Header.Set("Authorization", "Bearer access-1")
	forged.Header.Set("Content-Type", "application/json")
	forgedRecorder := httptest.NewRecorder()
	handler.ServeHTTP(forgedRecorder, forged)
	if forgedRecorder.Code != http.StatusConflict {
		t.Fatalf("非当前已选角色必须拒绝，实际=%d body=%s", forgedRecorder.Code, forgedRecorder.Body.String())
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
