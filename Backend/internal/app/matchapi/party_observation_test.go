// 本文件验证本人队伍观察的首次快照、跨服务通知、游标重同步和取消；内存仓储仅为测试替身。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
	"fmt"
	"testing"
	"time"
)

// TestPartyObservationInvitationAndMembership证明初始快照之后，另一服务实例提交的邀请和接受会唤醒观察，而不轮询Get。
func TestPartyObservationInvitationAndMembership(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	repo := NewMemoryPartyRepository()
	n := 0
	ids := func(prefix string) string { n++; return fmt.Sprintf("%s-%d", prefix, n) }
	first := NewService(repo, NewMemoryTicketRepository(), ids)
	second := NewService(repo, NewMemoryTicketRepository(), ids)
	created, err := first.CreateParty(ctx, "leader", "队长")
	if err != nil {
		t.Fatal(err)
	}
	initial, err := second.ObserveParty(ctx, "guest", "", 0)
	if err != nil {
		t.Fatal(err)
	}
	if initial.Snapshot.HasParty || len(initial.Invitations) != 0 || initial.Cursor == "" {
		t.Fatalf("首次快照错误: %+v", initial)
	}
	received := make(chan party.Observation, 1)
	failure := make(chan error, 1)
	go func() {
		v, e := second.ObserveParty(ctx, "guest", initial.Cursor, time.Second)
		if e != nil {
			failure <- e
			return
		}
		received <- v
	}()
	_, err = first.CommandParty(ctx, "leader", party.Command{PartyID: created.ID, Action: "invite", TargetPlayerID: "guest", MembershipRequestID: "inv-1", ClientRequestID: "invite-1", ExpectedRevision: created.Revision})
	if err != nil {
		t.Fatal(err)
	}
	var invited party.Observation
	select {
	case invited = <-received:
	case err = <-failure:
		t.Fatal(err)
	case <-ctx.Done():
		t.Fatal("邀请没有触发观察通知")
	}
	if len(invited.Invitations) != 1 || invited.Cursor == initial.Cursor {
		t.Fatalf("邀请通知错误: %+v", invited)
	}
	invitation := invited.Invitations[0]
	_, err = second.CommandParty(ctx, "guest", party.Command{PartyID: invitation.PartyID, Action: "acceptRequest", MembershipRequestID: invitation.RequestID, ClientRequestID: "accept-1", ExpectedRevision: invitation.Revision})
	if err != nil {
		t.Fatal(err)
	}
	joined, err := second.ObserveParty(ctx, "guest", invited.Cursor, 0)
	if err != nil {
		t.Fatal(err)
	}
	if !joined.Snapshot.HasParty || len(joined.Snapshot.Members) != 2 || len(joined.Invitations) != 0 {
		t.Fatalf("入队快照错误: %+v", joined)
	}
}

// TestPartyObservationCancellationAndCursorRecovery验证停止等待不会提交业务命令，未知游标必须给出完整重同步。
func TestPartyObservationCancellationAndCursorRecovery(t *testing.T) {
	service := NewService(NewMemoryPartyRepository(), NewMemoryTicketRepository(), func(string) string { return "party-a" })
	initial, err := service.ObserveParty(context.Background(), "one", "", 0)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err = service.ObserveParty(ctx, "one", initial.Cursor, time.Second); err == nil {
		t.Fatal("取消的观察不得成功")
	}
	recovered, err := service.ObserveParty(context.Background(), "one", "999999-0", 0)
	if err != nil {
		t.Fatal(err)
	}
	if !recovered.ResyncRequired || recovered.Snapshot.HasParty {
		t.Fatalf("未知游标没有重同步: %+v", recovered)
	}
	if _, err = service.ObserveParty(context.Background(), "one", "invalid", 0); err == nil {
		t.Fatal("非法游标必须拒绝")
	}
}

// TestPartyObservationExpiryNotification验证服务器依据真实到期安排一次通知，而不是由Widget计时器决定邀请有效性。
func TestPartyObservationExpiryNotification(t *testing.T) {
	repo := NewMemoryPartyRepository()
	p := party.New("expires", party.Member{PlayerID: "leader"})
	now := time.Now().UTC()
	if err := p.Invite("leader", "invite", "guest", now.Add(-party.InvitationLifetime+100*time.Millisecond)); err != nil {
		t.Fatal(err)
	}
	if err := repo.Save(context.Background(), p); err != nil {
		t.Fatal(err)
	}
	service := NewService(repo, NewMemoryTicketRepository(), func(string) string { return "unused" })
	first, err := service.ObserveParty(context.Background(), "guest", "", 0)
	if err != nil {
		t.Fatal(err)
	}
	if len(first.Invitations) != 1 {
		t.Fatal("测试邀请应尚未到期")
	}
	next, err := service.ObserveParty(context.Background(), "guest", first.Cursor, time.Second)
	if err != nil {
		t.Fatal(err)
	}
	if len(next.Invitations) != 0 || !next.Changed {
		t.Fatalf("邀请到期没有更新: %+v", next)
	}
}
