// 本回归在两个真实Registry Village候选上重复同一返回意图；不消费票，模拟成功响应在网络或上层保存时丢失。
package gameservercontrol

import (
	"context"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
	"testing"
	"time"
)

func TestWorldTransferUnknownResponseDoesNotReserveAgain(t *testing.T) {
	now := time.Now().UTC()
	clock := func() time.Time { return now }
	reg := gameserver.NewRegistry()
	s := NewService(reg, servertransfer.NewService([]byte("01234567890123456789012345678901"), clock), match.NewResultService(match.NewMemoryResultStore()), clock)
	for _, id := range []string{"village-a", "village-b"} {
		if err := s.Register(RegisterInput{GameID: "divine-beasts", GameServerID: id, ServerBootID: "boot-" + id, ServerRoleID: gc.RoleVillage, ExperienceID: gc.ExperienceVillageTutorial, RegionID: "test", WorldID: "test-village", PublicEndpoint: "127.0.0.1:17781", ProtocolVersion: 1, Capacity: 2}); err != nil {
			t.Fatal(err)
		}
		if err := s.SetReady(id); err != nil {
			t.Fatal(err)
		}
	}
	input := AllocateWorldTransferInput{World: AllocateWorldInput{ExperienceID: gc.ExperienceVillageTutorial, WorldID: "test-village", RegionID: "test", PlayerSlots: 1}, Transfer: IssueTransferInput{TicketID: "return-stable", GameID: "divine-beasts", PlayerID: "p", SessionID: "session", TTL: time.Minute}}
	first, err := s.AllocateWorldTransfer(context.Background(), input)
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 4; i++ {
		again, err := s.AllocateWorldTransfer(context.Background(), input)
		if err != nil {
			t.Fatal("同键未知响应必须恢复原目标", err)
		}
		if again.Ticket.Signature != first.Ticket.Signature || again.Ticket.SessionEpoch != first.Ticket.SessionEpoch || again.Assignment.GameServerID != first.Assignment.GameServerID {
			t.Fatal("恢复改变了准入意图")
		}
	}
	a, _ := reg.Get("village-a")
	b, _ := reg.Get("village-b")
	if a.ReservedPlayers+b.ReservedPlayers != 1 {
		t.Fatal("同一返回意图重复占容量", a.ReservedPlayers, b.ReservedPlayers)
	}
}
