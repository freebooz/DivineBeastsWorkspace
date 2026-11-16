package gameservercontrol

import (
	"context"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
	"testing"
	"time"
)

type queueOriginFixtureAuthority map[string]matchmaking.VillageQueueOriginEnrollment

func (a queueOriginFixtureAuthority) ValidateVillageQueueOrigin(_ context.Context, enrollment matchmaking.VillageQueueOriginEnrollment) (bool, error) {
	return a[enrollment.PlayerID] == enrollment, nil
}

// 同一队的两个玩家在各自位置排队，之后的重复报告不能移动原点，只有本人本轮返程票能取得它。
func TestVerifiedVillageReturnUsesEachPlayersFirstQueueOrigin(t *testing.T) {
	service, _, arena, match := lifecycleControlFixture(t)
	ctx := context.Background()
	village := RegisterInput{GameID: "divine-beasts", GameServerID: "village", ServerBootID: "village-boot", ServerRoleID: gc.RoleVillage, ExperienceID: gc.ExperienceVillageTutorial, RegionID: "region", WorldID: "village-world", PublicEndpoint: "127.0.0.1:17781", ProtocolVersion: 1, Capacity: 8}
	if err := service.Register(village); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(village.GameServerID); err != nil {
		t.Fatal(err)
	}
	world := AllocateWorldInput{ExperienceID: village.ExperienceID, WorldID: village.WorldID, RegionID: village.RegionID, PlayerSlots: 1}
	const queueID = "matchmaking-test-first"
	fixtures := []struct {
		player string
		x      float64
	}{{"one", 1234.25}, {"two", -437.5}}
	allocate := func(player, ticket, source string, origin *VillageQueueOriginRef) WorldTransferResult {
		t.Helper()
		result, err := service.AllocateWorldTransfer(ctx, AllocateWorldTransferInput{
			World: world, Transfer: IssueTransferInput{TicketID: ticket, GameID: "divine-beasts", PlayerID: player, SessionID: "session-" + player, SourceGameServerID: source, TTL: time.Minute}, VillageQueueOrigin: origin,
		})
		if err != nil {
			t.Fatal(err)
		}
		return result
	}
	authority := queueOriginFixtureAuthority{}
	service.ConfigureVillageQueueOriginAuthority(authority)
	for _, fixture := range fixtures {
		player := fixture.player
		session := "session-" + player
		initial := allocate(player, "initial-"+player, "", nil)
		accepted, err := service.ValidateTransfer(initial.Ticket, village.GameServerID)
		if err != nil || accepted.ReturnSpawn != nil {
			t.Fatalf("首次进村不应取得返程落点: %+v %v", accepted.ReturnSpawn, err)
		}
		origin := VillageQueueOriginInput{QueueTicketID: queueID, PlayerID: player, SessionID: session, SessionEpoch: accepted.SessionEpoch,
			Spawn: gc.WorldReturnSpawn{CharacterID: "character-" + player, MapPackage: "/Game/Maps/Village", Xcm: fixture.x, Ycm: 432, Zcm: 96, YawDegrees: 45}}
		authority[player] = matchmaking.VillageQueueOriginEnrollment{QueueTicketID: queueID, PlayerID: player, CharacterID: origin.Spawn.CharacterID, SessionID: session,
			SourceGameServerID: village.GameServerID, SourceExperienceID: village.ExperienceID, SourceServerBootID: village.ServerBootID, SourceSessionEpoch: accepted.SessionEpoch}
		if err := service.RecordVillageQueueOrigin(ctx, village.GameServerID, "wrong-boot", origin); err == nil {
			t.Fatal("错误Boot报告不应被接受")
		}
		if err := service.RecordVillageQueueOrigin(ctx, village.GameServerID, village.ServerBootID, origin); err != nil {
			t.Fatal(err)
		}
		origin.Spawn.Xcm = 999
		if err := service.RecordVillageQueueOrigin(ctx, village.GameServerID, village.ServerBootID, origin); err != nil {
			t.Fatal(err)
		}
		entry, err := service.IssueTransfer(IssueTransferInput{TicketID: "arena-" + player, GameID: "divine-beasts", PlayerID: player, SessionID: session, DestinationGameServerID: arena.GameServerID, MatchID: match.MatchID, TTL: time.Minute})
		if err != nil {
			t.Fatal(err)
		}
		if _, err := service.ValidateTransfer(entry, arena.GameServerID); err != nil {
			t.Fatal(err)
		}
		if err := service.RecordVillageQueueOrigin(ctx, village.GameServerID, village.ServerBootID, origin); err == nil {
			t.Fatal("玩家已迁出，旧Village不能再报告排队位置")
		}
	}
	if err := service.CancelArena(ctx, match.MatchID); err != nil {
		t.Fatal(err)
	}
	for _, fixture := range fixtures {
		player := fixture.player
		source := authority[player]
		ref := VillageQueueOriginRef{QueueTicketID: queueID, SourceGameServerID: village.GameServerID, SessionID: source.SessionID, CharacterID: source.CharacterID, SourceServerBootID: source.SourceServerBootID, SourceSessionEpoch: source.SourceSessionEpoch}
		returned := allocate(player, "return-"+player, arena.GameServerID, &ref)
		validated, err := service.ValidateTransfer(returned.Ticket, village.GameServerID)
		if err != nil || validated.ReturnSpawn == nil || validated.ReturnSpawn.Xcm != fixture.x || validated.ReturnSpawn.CharacterID != ref.CharacterID {
			t.Fatalf("%s返程落点=%+v，期望原点X=%v: %v", player, validated.ReturnSpawn, fixture.x, err)
		}
		ref.QueueTicketID = "matchmaking-test-another-attempt"
		fallback := allocate(player, "fallback-"+player, arena.GameServerID, &ref)
		validated, err = service.ValidateTransfer(fallback.Ticket, village.GameServerID)
		if err != nil || validated.ReturnSpawn != nil {
			t.Fatalf("其他报名尝试应保留标准出生: %+v %v", validated.ReturnSpawn, err)
		}
	}

	// 同名服务器已换Boot时仍可正常分配进村，但旧地图实例的落点不能通过新准入。
	village.ServerBootID = "village-new-boot"
	if err := service.Register(village); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(village.GameServerID); err != nil {
		t.Fatal(err)
	}
	returned, err := service.AllocateWorldTransfer(ctx, AllocateWorldTransferInput{
		World:              world,
		Transfer:           IssueTransferInput{TicketID: "return-after-boot-change", GameID: "divine-beasts", PlayerID: "one", SessionID: "session-one", SourceGameServerID: arena.GameServerID, TTL: time.Minute},
		VillageQueueOrigin: &VillageQueueOriginRef{QueueTicketID: queueID, SourceGameServerID: village.GameServerID, SessionID: "session-one", CharacterID: "character-one", SourceServerBootID: authority["one"].SourceServerBootID, SourceSessionEpoch: authority["one"].SourceSessionEpoch},
	})
	if err != nil {
		t.Fatal(err)
	}
	validated, err := service.ValidateTransfer(returned.Ticket, village.GameServerID)
	if err != nil || validated.ReturnSpawn != nil {
		t.Fatalf("新Boot应沿用标准出生: %+v %v", validated.ReturnSpawn, err)
	}
}
