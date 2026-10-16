// 本文件验证用户可触发的准备/匹配闭环到分配前门禁；未注入控制面时不能产生Assignment或迁移票据。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"fmt"
	"sync/atomic"
	"testing"
	"time"
)

// newLifecycleTestService明确使用测试配置，只用于纯应用回归，不登记正式地图或ProductionReady。
func newLifecycleTestService(t *testing.T) (*Service, *MemoryPartyRepository) {
	t.Helper()
	repo := NewMemoryPartyRepository()
	var id atomic.Int64
	service := NewService(repo, NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, id.Add(1)) })
	if err := service.ConfigureMatchmaking(MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only-arena", ContentRevision: "test-only-1", ProjectRuleRevision: 1, HeroCatalogRevision: 1, AllowedRegions: []string{"region"}}); err != nil {
		t.Fatal(err)
	}
	return service, repo
}
func lifecycleTestActor(player string) matchmaking.ActorContext {
	return matchmaking.ActorContext{PlayerID: player, SessionID: "session-" + player, CharacterID: "character-" + player, HeroDefinitionID: "Hero.Zodiac.Rat", CharacterRevision: 1}
}

// TestMatchmakingLifecycleSoloAndConfirmation证明两个单人无需创建队伍，逐人接受后才进入Allocating。
func TestMatchmakingLifecycleSoloAndConfirmation(t *testing.T) {
	service, repo := newLifecycleTestService(t)
	ctx := context.Background()
	for _, player := range []string{"one", "two"} {
		snapshot, err := service.CommandMatchmaking(ctx, lifecycleTestActor(player), matchmaking.Command{Action: "start", ClientRequestID: "start", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
		if err != nil {
			t.Fatal(err)
		}
		if snapshot.PartyID != "" {
			t.Fatal("单人不应被强制创建队伍")
		}
	}
	q, _ := repo.ReadMatchmaking(ctx)
	if len(q.Candidates) != 1 {
		t.Fatal("真实写入没有消费队列组成候选")
	}
	for _, player := range []string{"one", "two"} {
		state, err := service.ReadMatchmakingState(ctx, player)
		if err != nil {
			t.Fatal(err)
		}
		if !state.CanReply {
			t.Fatal("候选未发布给本人")
		}
		_, err = service.CommandMatchmaking(ctx, lifecycleTestActor(player), matchmaking.Command{Action: "reply", ClientRequestID: "accept", CandidateID: state.CandidateID, ExpectedRevision: state.CandidateRevision, Accept: true})
		if err != nil {
			t.Fatal(err)
		}
	}
	final, _ := service.ReadMatchmakingState(ctx, "one")
	if final.State != matchmaking.PhaseAllocating {
		t.Fatalf("全员接受后阶段错误: %+v", final)
	}
	if final.CanReply || final.CanCancel {
		t.Fatal("分配期间不能本地取消/再次确认")
	}
	if len(q.Reservations) != 2 {
		t.Fatal("迁移尚未完成不能提前释放预留")
	}
}

// TestMatchmakingLifecycleAllFivePartyModes证明限时准备绑定同一模式和名单，整队全部准备后才排队。
func TestMatchmakingLifecycleAllFivePartyModes(t *testing.T) {
	modes := []string{"Arena.Mode.Duel1v1", "Arena.Mode.Team2v2", "Arena.Mode.Team3v3", "Arena.Mode.Team4v4", "Arena.Mode.Team5v5"}
	for size := 2; size <= 5; size++ {
		t.Run(fmt.Sprint(size), func(t *testing.T) {
			service, repo := newLifecycleTestService(t)
			ctx := context.Background()
			for team := 0; team < 2; team++ {
				leader := fmt.Sprintf("leader-%d", team)
				p, err := service.CreateParty(ctx, leader, "队长")
				if err != nil {
					t.Fatal(err)
				}
				p, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
					for i := 1; i < size; i++ {
						if err := p.AddMember(party.Member{PlayerID: fmt.Sprintf("member-%d-%d", team, i), Online: true}); err != nil {
							return err
						}
					}
					return nil
				})
				if err != nil {
					t.Fatal(err)
				}
				state, err := service.CommandMatchmaking(ctx, lifecycleTestActor(leader), matchmaking.Command{Action: "start", ClientRequestID: "start", ExpectedRevision: p.Revision, PartyID: p.ID, ArenaModeID: modes[size-1], RegionID: "region"})
				if err != nil {
					t.Fatal(err)
				}
				if state.State != matchmaking.PhaseReadyCheck || !state.CanReady {
					t.Fatal("多人必须先进入准备")
				}
				current, _ := repo.Get(ctx, p.ID)
				if current.RosterLocked {
					t.Fatal("未全员准备不能锁死名单")
				}
				for i := 1; i < size; i++ {
					player := fmt.Sprintf("member-%d-%d", team, i)
					snapshot, _ := service.ReadMatchmakingState(ctx, player)
					if _, err := service.CommandMatchmaking(ctx, lifecycleTestActor(player), matchmaking.Command{Action: "ready", ClientRequestID: "ready", TicketID: snapshot.TicketID, ExpectedRevision: snapshot.Revision, Accept: true}); err != nil {
						t.Fatal(err)
					}
				}
				current, _ = repo.Get(ctx, p.ID)
				if !current.RosterLocked {
					t.Fatal("全部准备后名单必须锁定")
				}
			}
			state, _ := service.ReadMatchmakingState(ctx, "leader-0")
			if state.State != matchmaking.PhaseConfirmation || len(state.Participants) != 2*size {
				t.Fatalf("%dv%d未精确组成候选: %+v", size, size, state)
			}
			teams := map[string]string{}
			for _, slot := range state.Participants {
				teams[slot.PlayerID] = slot.TeamID
			}
			for team := 0; team < 2; team++ {
				side := teams[fmt.Sprintf("leader-%d", team)]
				for i := 1; i < size; i++ {
					if teams[fmt.Sprintf("member-%d-%d", team, i)] != side {
						t.Fatal("原队伍被拆分")
					}
				}
			}
		})
	}
}

// TestMatchmakingReadyTimeoutAndRosterChange验证期限和名单变化都会取消整轮准备，释放全员预留且不遗留锁。
func TestMatchmakingReadyTimeoutAndRosterChange(t *testing.T) {
	for _, change := range []string{"timeout", "roster"} {
		t.Run(change, func(t *testing.T) {
			service, repo := newLifecycleTestService(t)
			ctx := context.Background()
			p, _ := service.CreateParty(ctx, "leader", "队长")
			p, err := repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error { return p.AddMember(party.Member{PlayerID: "member", Online: true}) })
			if err != nil {
				t.Fatal(err)
			}
			state, err := service.CommandMatchmaking(ctx, lifecycleTestActor("leader"), matchmaking.Command{Action: "start", ClientRequestID: "start", ExpectedRevision: p.Revision, PartyID: p.ID, ArenaModeID: "Arena.Mode.Team2v2", RegionID: "region"})
			if err != nil {
				t.Fatal(err)
			}
			if change == "timeout" {
				err = repo.UpdateMatchmaking(ctx, nil, nil, func(tx *MatchmakingTransaction) error {
					entry := tx.Queue.Entries[state.TicketID]
					entry.DeadlineUTC = time.Now().Add(-time.Second)
					tx.Queue.Entries[state.TicketID] = entry
					return nil
				})
			} else {
				current, _ := repo.Get(ctx, p.ID)
				_, err = service.CommandParty(ctx, "member", party.Command{Action: "leave", PartyID: p.ID, ClientRequestID: "leave", ExpectedRevision: current.Revision})
			}
			if err != nil {
				t.Fatal(err)
			}
			if err := service.AdvanceMatchmaking(ctx); err != nil {
				t.Fatal(err)
			}
			final, _ := service.ReadMatchmakingState(ctx, "leader")
			if final.State != matchmaking.PhaseCancelled {
				t.Fatalf("准备没有取消: %+v", final)
			}
			q, _ := repo.ReadMatchmaking(ctx)
			if len(q.Reservations) != 0 {
				t.Fatal("准备终态仍保留预留")
			}
			current, _ := repo.Get(ctx, p.ID)
			if current.RosterLocked || current.ReadyCheckID != "" {
				t.Fatal("准备终态未恢复可编辑状态")
			}
		})
	}
}
