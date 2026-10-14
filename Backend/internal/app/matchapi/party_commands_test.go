// 本文件验证Match应用的权限、修订与跨队成员唯一归属；内存事务仅是测试替身，不代表Redis验证。
package matchapi

import (
	"context"
	"testing"

	"divinebeasts/backend/internal/modules/party"
)

// TestMatchRequestKeysAreScopedByPlayer防止另一玩家使用相同请求ID读取首位玩家的匹配票据。
func TestMatchRequestKeysAreScopedByPlayer(t *testing.T) {
	s := NewService(NewMemoryPartyRepository(), NewMemoryTicketRepository(), sequence("ticket-a", "ticket-b"))
	in := CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region", ClientRequestID: "same"}
	a, err := s.CreateMatchmakingTicket(context.Background(), "a", in)
	if err != nil {
		t.Fatal(err)
	}
	b, err := s.CreateMatchmakingTicket(context.Background(), "b", in)
	if err != nil {
		t.Fatal(err)
	}
	if a.TicketID == b.TicketID || len(b.PartyMemberIDs) != 1 || b.PartyMemberIDs[0] != "b" {
		t.Fatal("跨主体命中了同一票据")
	}
}

// TestMatchRequestRejectsChangedPayload防止使用同一主体请求ID改变模式／区域而静默返回旧结果。
func TestMatchRequestRejectsChangedPayload(t *testing.T) {
	s := NewService(NewMemoryPartyRepository(), NewMemoryTicketRepository(), sequence("ticket-a"))
	in := CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region-a", ClientRequestID: "same"}
	if _, err := s.CreateMatchmakingTicket(context.Background(), "a", in); err != nil {
		t.Fatal(err)
	}
	in.RegionID = "region-b"
	if _, err := s.CreateMatchmakingTicket(context.Background(), "a", in); err == nil {
		t.Fatal("异参重试没有冲突")
	}
}

// TestPartyCommandsDoNotAllowTwoPartyMembership验证两个邀请事务竞争时唯一玩家索引阻止双重归属。
func TestPartyCommandsDoNotAllowTwoPartyMembership(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryPartyRepository()
	s := NewService(repo, NewMemoryTicketRepository(), sequence("party-a", "party-b"))
	a, _ := s.CreateParty(ctx, "leader-a", "甲")
	b, _ := s.CreateParty(ctx, "leader-b", "乙")
	a, err := s.ApplyPartyCommand(ctx, "leader-a", PartyCommand{PartyID: a.ID, Action: "invite", TargetPlayerID: "guest", MembershipRequestID: "invite-a", ClientRequestID: "ca", ExpectedRevision: a.Revision})
	if err != nil {
		t.Fatal(err)
	}
	b, err = s.ApplyPartyCommand(ctx, "leader-b", PartyCommand{PartyID: b.ID, Action: "invite", TargetPlayerID: "guest", MembershipRequestID: "invite-b", ClientRequestID: "cb", ExpectedRevision: b.Revision})
	if err != nil {
		t.Fatal(err)
	}
	accepted, err := s.ApplyPartyCommand(ctx, "guest", PartyCommand{PartyID: a.ID, Action: "acceptRequest", MembershipRequestID: "invite-a", ClientRequestID: "accept-a", ExpectedRevision: a.Revision})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := s.ApplyPartyCommand(ctx, "guest", PartyCommand{PartyID: b.ID, Action: "acceptRequest", MembershipRequestID: "invite-b", ClientRequestID: "accept-b", ExpectedRevision: b.Revision}); err == nil {
		t.Fatal("同一玩家加入了两个队伍")
	}
	storedB, _ := repo.Get(ctx, b.ID)
	if len(storedB.Members) != 1 || storedB.MembershipRequests["invite-b"].State != party.MembershipRequestPending {
		t.Fatal("失败事务部分提交")
	}
	if len(accepted.Members) != 2 {
		t.Fatal("首次接受未提交")
	}
}

// TestPartyCommandIsIdempotentAndRejectsChangedIntent验证同一命令重复提交不推进修订，异参不能复用回执。
func TestPartyCommandIsIdempotentAndRejectsChangedIntent(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemoryPartyRepository(), NewMemoryTicketRepository(), sequence("party-a"))
	p, _ := s.CreateParty(ctx, "leader", "队长")
	in := PartyCommand{PartyID: p.ID, Action: "invite", TargetPlayerID: "guest", MembershipRequestID: "invite-a", ClientRequestID: "op", ExpectedRevision: p.Revision}
	a, err := s.ApplyPartyCommand(ctx, "leader", in)
	if err != nil {
		t.Fatal(err)
	}
	b, err := s.ApplyPartyCommand(ctx, "leader", in)
	if err != nil || b.Revision != a.Revision {
		t.Fatal("命令未幂等", err)
	}
	in.TargetPlayerID = "other"
	if _, err := s.ApplyPartyCommand(ctx, "leader", in); err == nil {
		t.Fatal("异参命令复用了回执")
	}
}
