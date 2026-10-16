// 本文件验证初次邀请原子创建、主体幂等及准备拒绝；不把单进程仓储当成Redis事务证明。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"sync"
	"testing"
)

func TestInitialInvitationAtomicAndIdempotent(t *testing.T) {
	repo := NewMemoryPartyRepository()
	service := NewService(repo, NewMemoryTicketRepository(), func(prefix string) string { return prefix + "-unused" })
	ctx := context.Background()
	command := party.Command{Action: "invite", TargetPlayerID: "target", ClientRequestID: "initial", ExpectedRevision: 0}
	var wg sync.WaitGroup
	var firstID string
	var mu sync.Mutex
	for i := 0; i < 16; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			result, err := service.CommandParty(ctx, "owner", command)
			if err != nil {
				t.Error(err)
				return
			}
			mu.Lock()
			defer mu.Unlock()
			if firstID == "" {
				firstID = result.AffectedPartyID
			}
			if firstID != result.AffectedPartyID || !result.Snapshot.HasParty || len(result.Snapshot.Requests) != 1 {
				t.Error("同一逻辑邀请必须恢复同一真实队伍和请求")
			}
		}()
	}
	wg.Wait()
	changed := command
	changed.TargetPlayerID = "other"
	if _, err := service.CommandParty(ctx, "owner", changed); err == nil {
		t.Fatal("同键异参必须拒绝")
	}
	changed.ClientRequestID = "another-command"
	if _, err := service.CommandParty(ctx, "owner", changed); err == nil {
		t.Fatal("已经有队伍时不能用空队伍盲写创建第二个队伍")
	}
	if len(repo.items) != 1 {
		t.Fatal("失败不能留下空队伍")
	}
	inbox, err := service.GetPartyInvitations(ctx, "target")
	if err != nil || len(inbox) != 1 {
		t.Fatal("邀请与真实队伍必须一起提交", err)
	}
}

func TestUnreadyMemberCanDeclineReadyCheck(t *testing.T) {
	service, repo := newLifecycleTestService(t)
	ctx := context.Background()
	value, err := service.CreateParty(ctx, "leader", "")
	if err != nil {
		t.Fatal(err)
	}
	value, err = repo.Update(ctx, value.ID, value.Revision, "", "", func(p *party.Party) error { return p.AddMember(party.Member{PlayerID: "member", Online: true}) })
	if err != nil {
		t.Fatal(err)
	}
	if _, err = service.CommandMatchmaking(ctx, lifecycleTestActor("leader"), matchmaking.Command{Action: "start", PartyID: value.ID, ExpectedRevision: value.Revision, ClientRequestID: "prepare", ArenaModeID: "Arena.Mode.Team2v2", RegionID: "region"}); err != nil {
		t.Fatal(err)
	}
	view, err := service.ReadPartyState(ctx, "member")
	if err != nil || view.ReadyCheckID == "" || view.ReadyDeadlineUTC == nil {
		t.Fatal("必须发布真实准备身份和截止时间", err)
	}
	if _, err = service.CommandParty(ctx, "member", party.Command{Action: "ready", PartyID: value.ID, ExpectedRevision: view.Revision, ClientRequestID: "decline", Ready: false}); err != nil {
		t.Fatal(err)
	}
	state, err := service.ReadMatchmakingState(ctx, "member")
	if err != nil || state.State != matchmaking.PhaseCancelled {
		t.Fatal("未准备成员明确拒绝必须结束本次整队准备", state.State, err)
	}
	view, _ = service.ReadPartyState(ctx, "member")
	if view.ReadyCheckID != "" || view.ReadyDeadlineUTC != nil || view.RosterLocked {
		t.Fatal("拒绝后必须清准备显示并释放名单")
	}
}
