// 本回归检查认证观察续租没有事件反馈循环，并在真实服务端截止使准备失效、转给最早在线成员。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"testing"
	"time"
)

func TestPartyPresenceLeaseAndOfflineLeader(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	ctx := context.Background()
	p, err := s.CreateParty(ctx, "leader", "队长")
	if err != nil {
		t.Fatal(err)
	}
	p, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
		if err := p.AddMember(party.Member{PlayerID: "first"}); err != nil {
			return err
		}
		return p.AddMember(party.Member{PlayerID: "second"})
	})
	if err != nil {
		t.Fatal(err)
	}
	initial, err := s.ObserveParty(ctx, "first", "", 0)
	if err != nil {
		t.Fatal(err)
	}
	before := time.Now()
	next, err := s.ObserveParty(ctx, "first", initial.Cursor, 30*time.Millisecond)
	if err != nil {
		t.Fatal(err)
	}
	if next.Changed || next.Cursor != initial.Cursor || time.Since(before) < 20*time.Millisecond {
		t.Fatal("仅续租不得立即触发可见状态或忙循环")
	}
	now := time.Now().UTC()
	p, _ = repo.GetByPlayerID(ctx, "leader")
	_, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
		m := p.Members["leader"]
		m.PresenceExpiresUTC = now.Add(-party.LeaderOfflineGrace - time.Second)
		m.Ready = true
		p.Members["leader"] = m
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if _, err = s.ObserveParty(ctx, "first", next.Cursor, 0); err != nil {
		t.Fatal(err)
	}
	p, _ = repo.GetByPlayerID(ctx, "first")
	if p.Members["leader"].Online || p.Members["leader"].Ready || p.LeaderPlayerID != "first" {
		t.Fatal("过期队长应离线失效准备并转给最早在线成员")
	}
	if _, err = s.ObserveParty(ctx, "leader", "", 0); err != nil {
		t.Fatal(err)
	}
	p, _ = repo.GetByPlayerID(ctx, "first")
	// 新一轮准备后成员失联必须整票取消，不能拆队留下其余人继续匹配。
	state, err := s.CommandMatchmaking(ctx, lifecycleTestActor("first"), matchmaking.Command{Action: "start", ClientRequestID: "presence-start", PartyID: p.ID, ExpectedRevision: p.Revision, ArenaModeID: "Arena.Mode.Team3v3", RegionID: "region"})
	if err != nil {
		t.Fatal(err)
	}
	p, _ = repo.GetByPlayerID(ctx, "first")
	_, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
		m := p.Members["second"]
		m.PresenceExpiresUTC = now.Add(-time.Second)
		p.Members["second"] = m
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if err = s.AdvanceMatchmaking(ctx); err != nil {
		t.Fatal(err)
	}
	state, err = s.ReadMatchmakingState(ctx, "first")
	if err != nil {
		t.Fatal(err)
	}
	if state.State != matchmaking.PhaseCancelled {
		t.Fatal("过期成员必须使整队准备取消", state.State)
	}
	p, _ = repo.GetByPlayerID(ctx, "first")
	if p.RosterLocked || p.ReadyCheckID != "" {
		t.Fatal("取消后必须释放队伍锁和准备会话")
	}
}
