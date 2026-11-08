package httpadapter

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

type transientResultStore struct {
	*match.MemoryResultStore
	fail bool
}

type transientResultProjection struct {
	*gameservercontrol.MemoryArenaLifecycleStore
	fail bool
}

func (s *transientResultProjection) SaveArena(ctx context.Context, record gameservercontrol.ArenaAssignmentRecord) error {
	if s.fail && record.State == "resultCommitted" {
		s.fail = false
		return errors.New("temporary control projection error")
	}
	return s.MemoryArenaLifecycleStore.SaveArena(ctx, record)
}

func (s *transientResultStore) InsertWithEvent(ctx context.Context, result match.StoredResult, event match.IntegrationEvent) error {
	if s.fail {
		s.fail = false
		return errors.New("temporary result database error")
	}
	return s.MemoryResultStore.InsertWithEvent(ctx, result, event)
}

func TestMatchResultHTTPRecoversAndReplaysAcknowledgementAfterRelease(t *testing.T) {
	ctx := context.Background()
	store := &transientResultStore{MemoryResultStore: match.NewMemoryResultStore(), fail: true}
	service := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now), match.NewResultService(store), time.Now)
	projection := &transientResultProjection{MemoryArenaLifecycleStore: gameservercontrol.NewMemoryArenaLifecycleStore(), fail: true}
	if err := service.ConfigureArenaLifecycle(projection); err != nil {
		t.Fatal(err)
	}
	const token = "result-test-only"
	registration := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "arena-result-test", ServerBootID: "result-boot", ServerRoleID: gc.RoleMainArena, ExperienceID: gc.ExperienceMainArenaMain, RegionID: "test", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", ProtocolVersion: 1, Capacity: 2}
	if err := service.Register(registration); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	assignment, err := service.AllocateMainArena(gameservercontrol.AllocateMainArenaInput{MatchID: "result-retry", ArenaModeID: gc.ArenaMode1v1, MapID: "test-arena", RegionID: registration.RegionID, Roster: []gc.RosterPlayer{{PlayerID: "a", TeamID: "team-a", CharacterID: "character-a"}, {PlayerID: "b", TeamID: "team-b", CharacterID: "character-b"}}})
	if err != nil {
		t.Fatal(err)
	}
	handler := NewGameServerControlHandler(service, token)
	entry, err := service.IssueTransfer(gameservercontrol.IssueTransferInput{TicketID: "enter-result-arena", GameID: registration.GameID, PlayerID: "a", SessionID: "active-session", DestinationGameServerID: registration.GameServerID, MatchID: assignment.MatchID, TTL: time.Minute})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateTransfer(entry, registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	client := NewGameServerControlClient(GameServerControlClientConfig{ClientConfig: ClientConfig{BaseURL: "http://control.test"}, BearerToken: token, DefaultRegion: "test"})
	client.client.Transport = testHandlerTransport{handler}
	_, err = client.AllocateWorldEntry(ctx, gateway.WorldEntryAllocationRequest{RequestID: "ordinary-while-active", GameID: registration.GameID, PlayerID: "a", SessionID: "new-session", CharacterID: "character-a", DesiredExperienceID: gc.ExperienceVillageMain})
	if status, code := gateway.OnlineErrorStatus(err); status != http.StatusConflict || code != "WORLD_MATCH_MIGRATION_REQUIRED" {
		t.Errorf("active arena conflict was lost across the Control HTTP client and Gateway mapping: %d %s", status, code)
	}
	post := func(result match.Result, boot string) *httptest.ResponseRecorder {
		t.Helper()
		body, err := json.Marshal(result)
		if err != nil {
			t.Fatal(err)
		}
		req := httptest.NewRequest(http.MethodPost, "/internal/v1/gameservers/match-result", bytes.NewReader(body))
		req.Header.Set("Authorization", "Bearer "+token)
		req.Header.Set("X-Game-Server-Id", registration.GameServerID)
		req.Header.Set("X-Game-Server-Boot-Id", boot)
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, req)
		return response
	}
	now := time.Now().UTC()
	result := match.Result{MatchID: assignment.MatchID, ArenaModeID: assignment.ArenaModeID, GameServerID: registration.GameServerID, StartedAt: now.Add(-time.Minute), EndedAt: now, WinningTeamID: "team-a", EndRevision: 1, EndReason: "Elimination", Teams: []match.TeamResult{{TeamID: "team-a", Won: true}, {TeamID: "team-b"}}, Players: []match.PlayerResult{{PlayerID: "a", TeamID: "team-a", CharacterID: "character-a"}, {PlayerID: "b", TeamID: "team-b", CharacterID: "character-b"}}}
	bad := result
	bad.Players = nil
	if response := post(bad, registration.ServerBootID); response.Code != http.StatusBadRequest {
		t.Errorf("invalid result status = %d, want 400", response.Code)
	}
	if response := post(result, registration.ServerBootID); response.Code != http.StatusServiceUnavailable {
		t.Errorf("temporary result store failure status = %d, want 503", response.Code)
	}
	if response := post(result, registration.ServerBootID); response.Code != http.StatusServiceUnavailable {
		t.Errorf("lost control projection status = %d, want 503", response.Code)
	}
	if err := service.CancelArena(ctx, assignment.MatchID); err == nil {
		t.Fatal("fault cancellation overrode an already durable result")
	}
	if record, found, err := projection.GetArenaByMatch(ctx, assignment.MatchID); err != nil || !found || record.State != "resultCommitted" || record.Result == nil {
		t.Fatalf("cancellation recovery lost the durable result: %+v %v", record, err)
	}
	first := post(result, registration.ServerBootID)
	if first.Code != http.StatusOK {
		t.Fatalf("retry status = %d", first.Code)
	}
	if err := service.Heartbeat(registration.GameServerID, 0, gameserver.StatusActive); err != nil {
		t.Fatal(err)
	}
	if err := service.FinalizeArena(ctx, assignment.MatchID); err != nil {
		t.Fatal(err)
	}
	repeated := post(result, registration.ServerBootID)
	if repeated.Code != http.StatusOK || repeated.Body.String() != first.Body.String() {
		t.Errorf("same-result replay after release = %d %s", repeated.Code, repeated.Body.String())
	}
	changed := result
	changed.EndRevision++
	if response := post(changed, registration.ServerBootID); response.Code != http.StatusConflict {
		t.Errorf("different result status = %d, want 409", response.Code)
	}
	registration.ServerBootID = "successor-boot"
	if err := service.Register(registration); err != nil {
		t.Fatal(err)
	}
	if response := post(result, registration.ServerBootID); response.Code != http.StatusConflict {
		t.Errorf("successor boot replay status = %d, want 409", response.Code)
	}
	if _, found := store.Event("match-completed:" + assignment.MatchID); !found {
		t.Error("result did not commit the existing Match.Completed outbox event")
	}
}
