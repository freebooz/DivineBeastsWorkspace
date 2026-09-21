package matchmaking

import "testing"

// BenchmarkNewTicket5v5（5v5匹配票据基准）衡量匹配热路径基础对象创建成本。
func BenchmarkNewTicket5v5(b *testing.B) {
	req := CreateTicketRequest{
		TicketID: "ticket-bench", GameID: "divine-beasts", ArenaModeID: "Arena.Mode.Team5v5", PartyID: "party-bench",
		PartyMemberIDs: []string{"p1", "p2", "p3", "p4", "p5"}, Region: "us-west",
	}
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		if _, err := NewTicket(req); err != nil {
			b.Fatal(err)
		}
	}
}
