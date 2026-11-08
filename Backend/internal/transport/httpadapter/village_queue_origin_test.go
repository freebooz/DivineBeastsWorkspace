package httpadapter

import (
	"bytes"
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchapi"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

type testHandlerTransport struct{ handler http.Handler }

func (t testHandlerTransport) RoundTrip(request *http.Request) (*http.Response, error) {
	response := httptest.NewRecorder()
	t.handler.ServeHTTP(response, request)
	return response.Result(), nil
}

// TestVillageQueueOriginHTTP保护专服报告、内部返程分配和目标签票消费的实际HTTP字段。
func TestVillageQueueOriginHTTP(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	const token = "village-queue-test-only"
	service := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now), match.NewResultService(match.NewMemoryResultStore()), time.Now)
	if err := service.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
		t.Fatal(err)
	}
	matchAuthority := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(prefix string) string { return prefix + "-http" })
	if err := matchAuthority.ConfigureMatchmaking(matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only-arena", ContentRevision: "test-only", ProjectRuleRevision: 1, AllowedRegions: []string{"test"}}); err != nil {
		t.Fatal(err)
	}
	matchClient := NewMatchClient(ClientConfig{BaseURL: "http://match.test", BearerToken: token})
	matchClient.client.Transport = testHandlerTransport{NewMatchHandler(matchAuthority, token)}
	service.ConfigureVillageQueueOriginAuthority(matchClient)
	village := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "village-http", ServerBootID: "village-http-boot", ServerRoleID: gc.RoleVillage, ExperienceID: gc.ExperienceVillageMain, WorldID: "divinebeasts.world.village_main@1", RegionID: "test", PublicEndpoint: "127.0.0.1:17781", ProtocolVersion: 1, Capacity: 8}
	if err := service.Register(village); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(village.GameServerID); err != nil {
		t.Fatal(err)
	}
	handler := NewGameServerControlHandler(service, token)
	client := NewGameServerControlClient(GameServerControlClientConfig{ClientConfig: ClientConfig{BaseURL: "http://control.test"}, BearerToken: token, DefaultRegion: "test"})
	client.client.Transport = testHandlerTransport{handler}
	input := gameservercontrol.AllocateWorldTransferInput{
		World:    gameservercontrol.AllocateWorldInput{ExperienceID: village.ExperienceID, WorldID: village.WorldID, RegionID: village.RegionID, PlayerSlots: 1},
		Transfer: gameservercontrol.IssueTransferInput{TicketID: "first-village-admission", GameID: village.GameID, PlayerID: "test-player", SessionID: "test-session", TTL: time.Minute},
	}
	initial, err := client.AllocateWorldTransfer(ctx, input)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = service.ValidateTransferContext(ctx, initial.Ticket, village.GameServerID); err != nil {
		t.Fatal(err)
	}
	actor := matchmaking.ActorContext{PlayerID: "test-player", SessionID: "test-session", CharacterID: "test-character", HeroDefinitionID: "hero-test", CharacterRevision: 1,
		SourceGameServerID: village.GameServerID, SourceExperienceID: village.ExperienceID, SourceServerBootID: village.ServerBootID, SourceSessionEpoch: initial.Ticket.SessionEpoch}
	queued, err := matchAuthority.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "start", ClientRequestID: "test-start", ArenaModeID: gc.ArenaMode1v1, RegionID: "test"})
	if err != nil || queued.TicketID != "matchmaking-http" {
		t.Fatalf("expected an actual Match enrollment, got %+v: %v", queued, err)
	}
	call := func(path, auth, boot string, body []byte) *httptest.ResponseRecorder {
		t.Helper()
		req := httptest.NewRequest(http.MethodPost, path, bytes.NewReader(body)).WithContext(ctx)
		req.Header.Set("Authorization", auth)
		req.Header.Set("X-Game-Server-Id", village.GameServerID)
		req.Header.Set("X-Game-Server-Boot-Id", boot)
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, req)
		return response
	}
	const recordPath = "/internal/v1/gameservers/village-queue-origin"
	const report = `{"queueTicketId":"matchmaking-http","playerId":"test-player","sessionId":"test-session","sessionEpoch":"1","spawn":{"characterId":"test-character","mapPackage":"/Game/Village","xCm":12.25,"yCm":-107,"zCm":264,"yawDegrees":29}}`
	if r := call(recordPath, "", village.ServerBootID, []byte(report)); r.Code != http.StatusUnauthorized {
		t.Fatalf("匿名报告应拒绝，状态=%d", r.Code)
	}
	if r := call(recordPath, "Bearer "+token, "wrong-boot", []byte(report)); r.Code != http.StatusConflict {
		t.Fatalf("错误源Boot应拒绝，状态=%d", r.Code)
	}
	if r := call(recordPath, "Bearer "+token, village.ServerBootID, []byte(strings.Replace(report, "matchmaking-http", "matchmaking-forged", 1))); r.Code == http.StatusOK {
		t.Error("合法格式的伪造报名不可写入落点")
	}
	if r := call(recordPath, "Bearer "+token, village.ServerBootID, []byte(strings.Replace(report, "test-character", "wrong-character", 1))); r.Code == http.StatusOK {
		t.Error("报名锁定角色以外的落点不可写入")
	}
	if r := call(recordPath, "Bearer "+token, village.ServerBootID, []byte(report)); r.Code != http.StatusOK {
		t.Fatalf("可信专服报告应成功，状态=%d", r.Code)
	}
	input.Transfer.TicketID = "village-return-admission"
	input.Transfer.SourceGameServerID = "source-arena"
	input.World.RegionID = "arena-match-region"
	input.VillageQueueOrigin = &gameservercontrol.VillageQueueOriginRef{QueueTicketID: "matchmaking-http", SourceGameServerID: village.GameServerID, SessionID: "test-session", CharacterID: "test-character", SourceServerBootID: village.ServerBootID, SourceSessionEpoch: initial.Ticket.SessionEpoch}
	returned, err := client.AllocateWorldTransfer(ctx, input)
	if err != nil || returned.Assignment.GameServerID != village.GameServerID || returned.Assignment.RegionID != village.RegionID {
		t.Fatalf("应优先返回仍可用的原村实例及其区域: %v", err)
	}
	validate := func(ticket servertransfer.Ticket) *httptest.ResponseRecorder {
		t.Helper()
		body, err := json.Marshal(map[string]any{"ticket": ticket, "destinationGameServerId": village.GameServerID})
		if err != nil {
			t.Fatal(err)
		}
		return call("/internal/v1/gameservers/validate-transfer", "Bearer "+token, village.ServerBootID, body)
	}
	invalid := returned.Ticket
	invalid.Signature = "invalid-signature"
	if r := validate(invalid); r.Code != http.StatusUnauthorized {
		t.Fatalf("无效返程票据必须拒绝，状态=%d", r.Code)
	}
	valid := validate(returned.Ticket)
	var result servertransfer.ValidationResult
	if err = json.NewDecoder(valid.Body).Decode(&result); valid.Code != http.StatusOK || err != nil || result.ReturnSpawn == nil || result.ReturnSpawn.Xcm != 12.25 {
		t.Fatalf("目标服务器未收到已验证的精确落点，状态=%d，错误=%v", valid.Code, err)
	}
}
