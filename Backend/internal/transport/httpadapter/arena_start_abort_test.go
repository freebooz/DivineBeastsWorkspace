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
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

func TestArenaStartAbortHTTPFencesBootAndClosesAdmission(t *testing.T) {
	ctx := context.Background()
	resultStore := match.NewMemoryResultStore()
	now := time.Now().UTC()
	clock := func() time.Time { return now }
	service := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), clock), match.NewResultService(resultStore), clock)
	if err := service.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
		t.Fatal(err)
	}
	registration := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "test-arena", ServerBootID: "test-boot", ServerRoleID: gc.RoleMainArena, ExperienceID: gc.ExperienceMainArenaMain, RegionID: "test", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", ProtocolVersion: 1, Capacity: 2}
	if err := service.Register(registration); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	assignment, err := service.AllocateMainArena(gameservercontrol.AllocateMainArenaInput{MatchID: "abort-test", ArenaModeID: gc.ArenaMode1v1, MapID: "test-arena", RegionID: registration.RegionID, Roster: []gc.RosterPlayer{{PlayerID: "a", TeamID: "team-a", CharacterID: "character-a"}, {PlayerID: "b", TeamID: "team-b", CharacterID: "character-b"}}})
	if err != nil {
		t.Fatal(err)
	}
	input := gameservercontrol.IssueTransferInput{TicketID: "enter-arena-before-abort", GameID: registration.GameID, PlayerID: "a", SessionID: "active-session", DestinationGameServerID: registration.GameServerID, MatchID: assignment.MatchID, TTL: time.Minute}
	ticket, err := service.IssueTransferContext(ctx, input)
	if err != nil {
		t.Fatal(err)
	}
	const bearer = "test-bearer"
	handler := NewGameServerControlHandler(service, bearer)
	post := func(token, boot, headerBoot, reason string) *httptest.ResponseRecorder {
		t.Helper()
		body, err := json.Marshal(map[string]string{"matchId": assignment.MatchID, "gameServerId": registration.GameServerID, "serverBootId": boot, "reason": reason})
		if err != nil {
			t.Fatal(err)
		}
		req := httptest.NewRequest(http.MethodPost, "/internal/v1/gameservers/match-start-abort", bytes.NewReader(body))
		req.Header.Set("Authorization", "Bearer "+token)
		req.Header.Set("X-Game-Server-Id", registration.GameServerID)
		req.Header.Set("X-Game-Server-Boot-Id", headerBoot)
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, req)
		return response
	}
	for _, item := range []struct {
		token, boot, headerBoot, reason string
		status                          int
	}{
		{"wrong", registration.ServerBootID, registration.ServerBootID, "spawn_failed", http.StatusUnauthorized},
		{bearer, registration.ServerBootID, "wrong-header", "spawn_failed", http.StatusForbidden},
		{bearer, "stale-boot", "stale-boot", "spawn_failed", http.StatusConflict},
		{bearer, registration.ServerBootID, registration.ServerBootID, "unrecognized", http.StatusBadRequest},
	} {
		if response := post(item.token, item.boot, item.headerBoot, item.reason); response.Code != item.status {
			t.Errorf("abort rejection status = %d, want %d", response.Code, item.status)
		}
	}
	if response := post(bearer, registration.ServerBootID, registration.ServerBootID, "spawn_failed"); response.Code != http.StatusOK {
		t.Fatalf("trusted abort status = %d, want 200", response.Code)
	}
	first, err := service.GetArenaStatus(ctx, assignment.MatchID)
	if err != nil || !first.ServerAlive || first.Record.State != "cancelled" || first.Record.Result != nil {
		t.Fatalf("trusted abort must durably cancel a healthy arena without constructing a result: state=%s err=%v", first.Record.State, err)
	}
	if response := post(bearer, registration.ServerBootID, registration.ServerBootID, "spawn_failed"); response.Code != http.StatusOK {
		t.Errorf("exact retry status = %d, want 200", response.Code)
	}
	if response := post(bearer, registration.ServerBootID, registration.ServerBootID, "player_disconnected"); response.Code != http.StatusConflict {
		t.Errorf("conflicting retry status = %d, want 409", response.Code)
	}
	second, err := service.GetArenaStatus(ctx, assignment.MatchID)
	if err != nil || !second.Record.UpdatedAtUTC.Equal(first.Record.UpdatedAtUTC) {
		t.Errorf("retry changed first abort time: %v", err)
	}
	if _, err := service.ValidateTransferContext(ctx, ticket, registration.GameServerID); err == nil {
		t.Error("already issued entry ticket admitted after abort")
	}
	if _, found := resultStore.Event("match-completed:" + assignment.MatchID); found {
		t.Error("start abort fabricated a match-completed result")
	}
	if err := service.FinalizeArena(ctx, assignment.MatchID); err == nil {
		t.Error("abort finalized before a fresh zero-player heartbeat")
	}
	now = now.Add(time.Second)
	if err := service.Heartbeat(registration.GameServerID, 0, gameserver.StatusActive); err != nil {
		t.Fatal(err)
	}
	if err := service.FinalizeArena(ctx, assignment.MatchID); err == nil {
		t.Fatal("late zero-player heartbeat cannot finalize a live aborted arena without its authenticated drain")
	}
	postDrain := func(boot string) int {
		t.Helper()
		body, err := json.Marshal(map[string]string{"gameServerId": registration.GameServerID})
		if err != nil {
			t.Fatal(err)
		}
		req := httptest.NewRequest(http.MethodPost, "/internal/v1/gameservers/drain", bytes.NewReader(body))
		req.Header.Set("Authorization", "Bearer "+bearer)
		req.Header.Set("X-Game-Server-Id", registration.GameServerID)
		req.Header.Set("X-Game-Server-Boot-Id", boot)
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, req)
		return response.Code
	}
	now = now.Add(time.Second)
	if status := postDrain("stale-boot"); status != http.StatusConflict {
		t.Fatalf("drain from old Boot: got %d, want 409", status)
	}
	if err := service.FinalizeArena(ctx, assignment.MatchID); err == nil {
		t.Fatal("drain from old Boot cannot finalize the current arena")
	}
	if status := postDrain(registration.ServerBootID); status != http.StatusOK {
		t.Fatalf("drain from current Boot: got %d, want 200", status)
	}
	if err := service.FinalizeArena(ctx, assignment.MatchID); err != nil {
		t.Fatal(err)
	}
	if response := post(bearer, registration.ServerBootID, registration.ServerBootID, "spawn_failed"); response.Code != http.StatusOK {
		t.Errorf("exact retry after release status = %d, want 200", response.Code)
	}
}
