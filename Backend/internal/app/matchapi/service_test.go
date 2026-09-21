package matchapi

import (
	"context"
	"testing"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/party"
)

// TestCreatePartyAndMatchmakingTicketLocksRoster（Party进入匹配锁名单测试）验证Party作为原子匹配单元进入Queue后成员名单被冻结。
func TestCreatePartyAndMatchmakingTicketLocksRoster(t *testing.T) {
	parties := NewMemoryPartyRepository()
	tickets := NewMemoryTicketRepository()
	ids := sequence("party-1", "mm-1")
	service := NewService(parties, tickets, ids)

	created, err := service.CreateParty(context.Background(), "leader-1", "队长")
	if err != nil {
		t.Fatal(err)
	}
	if err := created.AddMember(party.Member{PlayerID: "member-2", DisplayName: "队友", Online: true}); err != nil {
		t.Fatal(err)
	}
	if err := parties.Save(context.Background(), created); err != nil {
		t.Fatal(err)
	}

	ticket, err := service.CreateMatchmakingTicket(context.Background(), "leader-1", CreateMatchmakingTicketInput{ArenaModeID: gameservercontract.ArenaMode3v3, PartyID: "party-1", RegionID: "us-west", ClientRequestID: "client-1"})
	if err != nil {
		t.Fatal(err)
	}
	if ticket.PartySize != 2 || len(ticket.PartyMemberIDs) != 2 {
		t.Fatalf("Ticket成员错误: %+v", ticket)
	}
	storedParty, err := parties.Get(context.Background(), "party-1")
	if err != nil {
		t.Fatal(err)
	}
	if !storedParty.RosterLocked || storedParty.SelectedArenaModeID != gameservercontract.ArenaMode3v3 {
		t.Fatalf("Party应已锁定: %+v", storedParty)
	}
}

// TestMatchmakingClientRequestIsIdempotent（匹配请求幂等测试）验证同一ClientRequestID重试返回同一Ticket。
func TestMatchmakingClientRequestIsIdempotent(t *testing.T) {
	service := NewService(NewMemoryPartyRepository(), NewMemoryTicketRepository(), sequence("mm-1", "mm-2"))
	input := CreateMatchmakingTicketInput{ArenaModeID: gameservercontract.ArenaMode1v1, RegionID: "us-west", ClientRequestID: "client-1"}
	first, err := service.CreateMatchmakingTicket(context.Background(), "p1", input)
	if err != nil {
		t.Fatal(err)
	}
	second, err := service.CreateMatchmakingTicket(context.Background(), "p1", input)
	if err != nil {
		t.Fatal(err)
	}
	if first.TicketID != second.TicketID {
		t.Fatalf("幂等请求必须返回同一Ticket: %s != %s", first.TicketID, second.TicketID)
	}
}

func sequence(values ...string) func(string) string {
	index := 0
	return func(_ string) string { value := values[index]; index++; return value }
}
