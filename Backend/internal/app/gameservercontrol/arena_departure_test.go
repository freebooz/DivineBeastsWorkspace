package gameservercontrol

import (
	"context"
	"errors"
	"testing"
	"time"

	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/match"
)

func TestArenaDepartureRequiresAuthorizedReturnAtIssueAndAdmission(t *testing.T) {
	service, store, arena, assignment := lifecycleControlFixture(t)
	ctx := context.Background()
	village := RegisterInput{GameID: arena.GameID, GameServerID: "departure-village", ServerBootID: "departure-boot", ServerRoleID: gc.RoleVillage, ExperienceID: gc.ExperienceVillageMain, RegionID: arena.RegionID, WorldID: "World.DepartureVillage", PublicEndpoint: "127.0.0.1:17781", ProtocolVersion: 1, Capacity: 8}
	if err := service.Register(village); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(village.GameServerID); err != nil {
		t.Fatal(err)
	}
	ordinary := AllocateWorldTransferInput{
		World:    AllocateWorldInput{ExperienceID: village.ExperienceID, WorldID: village.WorldID, RegionID: village.RegionID, PlayerSlots: 1},
		Transfer: IssueTransferInput{TicketID: "ordinary-preissued", GameID: arena.GameID, PlayerID: "one", SessionID: "renewed-session-one", TTL: time.Minute},
	}
	arenaTicket, err := service.IssueTransfer(IssueTransferInput{TicketID: "enter-active-arena", GameID: arena.GameID, PlayerID: "one", SessionID: "session-one", DestinationGameServerID: arena.GameServerID, MatchID: assignment.MatchID, TTL: time.Minute})
	if err != nil {
		t.Fatal(err)
	}
	preissued, err := service.AllocateWorldTransfer(ctx, ordinary)
	if err != nil || preissued.Ticket.IssuedAt.Before(arenaTicket.IssuedAt) {
		t.Fatalf("fixture needs a newer ordinary ticket in another session before arena admission: %+v, %v", preissued.Ticket, err)
	}
	if _, err = service.ValidateTransfer(arenaTicket, arena.GameServerID); err != nil {
		t.Fatal(err)
	}
	if _, err = service.ValidateTransfer(preissued.Ticket, village.GameServerID); !errors.Is(err, ErrWorldMatchMigrationRequired) {
		t.Error("a pre-issued ordinary ticket bypassed current MainArena authority")
	}
	forged := preissued.Ticket
	forged.SourceMatchID = assignment.MatchID
	if _, err = service.ValidateTransfer(forged, village.GameServerID); err == nil || errors.Is(err, ErrWorldMatchMigrationRequired) {
		t.Fatalf("an ordinary ticket gained arena exit authority without a new HMAC: %v", err)
	}
	staged := PlayerPresence{PlayerID: "one", SessionID: ordinary.Transfer.SessionID, GameServerID: village.GameServerID, ServerBootID: village.ServerBootID, TicketID: preissued.Ticket.TicketID, SessionEpoch: preissued.Ticket.SessionEpoch, IssuedAtUTC: preissued.Ticket.IssuedAt, AcceptedAtUTC: time.Now().UTC()}
	if err = store.SavePresence(ctx, staged, ""); !errors.Is(err, ErrWorldMatchMigrationRequired) {
		t.Fatalf("presence writer must recheck the arena when admission races a world issue: %v", err)
	}
	ordinary.Transfer.TicketID = "ordinary-during-active-arena"
	ordinary.Transfer.SessionID = "renewed-session-one"
	if _, err = service.AllocateWorldTransfer(ctx, ordinary); err == nil {
		t.Error("a renewed session bypassed an active MainArena")
	}
	ordinary.Transfer.SourceGameServerID = arena.GameServerID
	if _, err = service.AllocateWorldTransfer(ctx, ordinary); err == nil {
		t.Error("an active arena cannot authorize a return by source server ID alone")
	}
	result := match.Result{MatchID: assignment.MatchID, ArenaModeID: assignment.ArenaModeID, GameServerID: arena.GameServerID, StartedAt: time.Now().Add(-time.Minute).UTC(), EndedAt: time.Now().UTC(), WinningTeamID: "team-a", Teams: []match.TeamResult{{TeamID: "team-a", Won: true}, {TeamID: "team-b"}}, Players: []match.PlayerResult{{PlayerID: "one", TeamID: "team-a", CharacterID: "character-one"}, {PlayerID: "two", TeamID: "team-b", CharacterID: "character-two"}}}
	if _, err = service.SubmitMatchResult(ctx, result); err != nil {
		t.Fatal(err)
	}
	returning := ordinary
	returning.Transfer.TicketID = "authorized-arena-return"
	returning.Transfer.SessionID = "session-one"
	returned, err := service.AllocateWorldTransfer(ctx, returning)
	if err != nil {
		t.Fatal(err)
	}
	if returned.Ticket.SourceMatchID != assignment.MatchID {
		t.Fatal("authorized return omitted its signed source match")
	}
	tampered := returned.Ticket
	tampered.SourceMatchID = "different-arena"
	if _, err = service.ValidateTransfer(tampered, village.GameServerID); err == nil {
		t.Fatal("a return ticket for another match was accepted")
	}
	if _, err = service.ValidateTransfer(returned.Ticket, village.GameServerID); err != nil {
		t.Fatalf("settled arena return failed: %v", err)
	}
	ordinary.Transfer.TicketID = "ordinary-after-return"
	ordinary.Transfer.SourceGameServerID = ""
	if _, err = service.AllocateWorldTransfer(ctx, ordinary); err != nil {
		t.Fatalf("ordinary admission did not recover after returning: %v", err)
	}
}
