// 本文件验证持久步骤的跨工作者租约、栅栏和事务失败边界；不以内存测试冒充PG恢复验收。
package matchservice

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"errors"
	"testing"
	"time"
)

// OperationFixture只生成明确测试的已确认1v1名单，不能装配到运行服务。
func OperationFixture(now time.Time) Operation {
	value := Operation{ID: "candidate-test", MatchID: "match-test", MapID: "map-test", RuleRevision: 1, ContentRevision: "test-only", HeroCatalogRevision: 1, Stage: "allocating", CreatedAtUTC: now, DeadlineUTC: now.Add(time.Minute), Contexts: map[string]matchmaking.ActorContext{}, TicketIDs: map[string]string{}, Tickets: nil, Returns: map[string]ReturnIntent{}}
	value.Candidate = matchmaking.Candidate{ID: value.ID, GameID: "divine-beasts", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "test", Roster: []matchmaking.CandidateSlot{{PlayerID: "a", TeamID: "team-a", Accepted: true}, {PlayerID: "b", TeamID: "team-b", Accepted: true}}}
	// 测试只关心秘密容器真实存在，绝不构造有效签名或固定准入成功。
	value.Tickets = map[string]servertransfer.Ticket{}
	for _, slot := range value.Candidate.Roster {
		value.Contexts[slot.PlayerID] = matchmaking.ActorContext{PlayerID: slot.PlayerID, SessionID: "session-" + slot.PlayerID, CharacterID: "character-" + slot.PlayerID, HeroDefinitionID: "hero-test", CharacterRevision: 1}
		value.TicketIDs[slot.PlayerID] = "transfer-" + slot.PlayerID
	}
	value.InputFingerprint, _ = OperationInputFingerprint(value)
	return value
}

func TestOperationLeaseFencesLateWorker(t *testing.T) {
	ctx := context.Background()
	now := time.Date(2026, 10, 15, 0, 0, 0, 0, time.UTC)
	store := NewMemoryOperationStore()
	proposal := OperationFixture(now)
	first, err := store.ClaimOperation(ctx, proposal, "worker-a", now, 10*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = store.ClaimOperation(ctx, proposal, "worker-b", now, 10*time.Second); !errors.Is(err, ErrOperationBusy) {
		t.Fatal("有效租约必须拒绝第二拥有者", err)
	}
	successor, err := store.ClaimOperation(ctx, proposal, "worker-b", now.Add(11*time.Second), 10*time.Second)
	if err != nil || successor.Fence <= first.Fence {
		t.Fatal("到期后必须递增栅栏", err)
	}
	first.Stage = "signing"
	if _, err = store.SaveOperation(ctx, first, now.Add(12*time.Second), 10*time.Second); !errors.Is(err, ErrOperationLeaseLost) {
		t.Fatal("迟到工作者不得保存", err)
	}
	if err = store.ReleaseOperation(ctx, first.ID, first.LeaseOwner, first.Fence); !errors.Is(err, ErrOperationLeaseLost) {
		t.Fatal("迟到释放不得撤销后继", err)
	}
	successor.Stage = "signing"
	saved, err := store.SaveOperation(ctx, successor, now.Add(12*time.Second), 10*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	stale := saved
	stale.Stage = "allocating"
	if _, err = store.SaveOperation(ctx, stale, now.Add(13*time.Second), 10*time.Second); err == nil {
		t.Fatal("步骤不得倒退")
	}
	snapshot, _, _ := store.GetOperation(ctx, saved.ID)
	if snapshot.Stage != "signing" || snapshot.Version != saved.Version {
		t.Fatal("失败必须无部分写入")
	}
	snapshot.Contexts["a"] = matchmaking.ActorContext{}
	again, _, _ := store.GetOperation(ctx, saved.ID)
	if again.Contexts["a"].PlayerID != "a" {
		t.Fatal("读取不能泄漏可变共享引用")
	}
}
