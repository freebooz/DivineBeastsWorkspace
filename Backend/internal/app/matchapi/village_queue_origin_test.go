package matchapi

import (
	"context"
	"testing"
	"time"

	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
)

func TestVillageQueueOriginRequiresCurrentMatchEnrollment(t *testing.T) {
	ctx := context.Background()
	service, repository := newLifecycleTestService(t)
	actor := lifecycleTestActor("queue-origin-player")
	actor.SourceGameServerID = "village-source"
	actor.SourceExperienceID = gc.ExperienceVillageMain
	actor.SourceServerBootID = "village-boot"
	actor.SourceSessionEpoch = 17
	state, err := service.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "start", ClientRequestID: "origin-start", ArenaModeID: gc.ArenaMode1v1, RegionID: "region"})
	if err != nil {
		t.Fatal(err)
	}
	proof := matchmaking.VillageQueueOriginEnrollment{QueueTicketID: state.TicketID, PlayerID: actor.PlayerID, CharacterID: actor.CharacterID, SessionID: actor.SessionID,
		SourceGameServerID: actor.SourceGameServerID, SourceExperienceID: actor.SourceExperienceID, SourceServerBootID: actor.SourceServerBootID, SourceSessionEpoch: actor.SourceSessionEpoch}
	check := func(proof matchmaking.VillageQueueOriginEnrollment, want bool) {
		t.Helper()
		got, err := service.ValidateVillageQueueOrigin(ctx, proof)
		if err != nil || got != want {
			t.Fatalf("expected authorization %v for %+v, got %v: %v", want, proof, got, err)
		}
	}
	check(proof, true)
	for _, change := range []func(*matchmaking.VillageQueueOriginEnrollment){
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.QueueTicketID = "matchmaking-forged" },
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.PlayerID = "other-player" },
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.CharacterID = "other-character" },
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.SessionID = "renewed-session" },
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.SourceServerBootID = "new-boot" },
		func(p *matchmaking.VillageQueueOriginEnrollment) { p.SourceSessionEpoch++ },
	} {
		changed := proof
		change(&changed)
		check(changed, false)
	}
	queue, err := repository.ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	if villageQueueOriginEnrollmentActive(queue, proof, queue.Entries[state.TicketID].DeadlineUTC.Add(time.Nanosecond)) {
		t.Error("expired enrollment accepted before the cleanup worker processed its deadline")
	}
	if _, err := service.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "cancel", ClientRequestID: "origin-cancel", TicketID: state.TicketID, ExpectedRevision: state.Revision}); err != nil {
		t.Fatal(err)
	}
	check(proof, false)
}
