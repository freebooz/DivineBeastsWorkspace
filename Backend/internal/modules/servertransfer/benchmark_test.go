package servertransfer

import (
	"fmt"
	"testing"
	"time"
)

// BenchmarkIssueAndValidate（迁移票据签发验证基准）衡量HMAC签名和一次性校验成本。
func BenchmarkIssueAndValidate(b *testing.B) {
	now := time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)
	service := NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		ticket, err := service.Issue(IssueRequest{
			TicketID: fmt.Sprintf("ticket-%d", i), GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1",
			DestinationGameServerID: "arena-1", DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.MainArena", TTL: 30 * time.Second,
		})
		if err != nil {
			b.Fatal(err)
		}
		if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-1"}); err != nil {
			b.Fatal(err)
		}
	}
}
