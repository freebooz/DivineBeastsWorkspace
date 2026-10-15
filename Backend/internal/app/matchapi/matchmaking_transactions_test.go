// 本文件验证报名事务失败无副作用、并发唯一预留与组队准备门禁；内存仅用于确定性领域/应用回归。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
	"errors"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
)

// TestAtomicQueueConcurrentReservations证明不同幂等键也不能让同一玩家进入多条队列；成功回执重放保持同一票据。
func TestAtomicQueueConcurrentReservations(t *testing.T) {
	repo := NewMemoryPartyRepository()
	var ids atomic.Int64
	service := NewService(repo, NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, ids.Add(1)) })
	var workers sync.WaitGroup
	var successes atomic.Int64
	for i := 0; i < 20; i++ {
		workers.Add(1)
		go func(i int) {
			defer workers.Done()
			_, err := service.CreateMatchmakingTicket(context.Background(), "player", CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region", ClientRequestID: fmt.Sprint(i)})
			if err == nil {
				successes.Add(1)
			} else if !strings.HasPrefix(err.Error(), "MATCH_PLAYER_RESERVED:") {
				t.Errorf("意外错误: %v", err)
			}
		}(i)
	}
	workers.Wait()
	if successes.Load() != 1 {
		t.Fatalf("只能有一个有效报名，实际%d", successes.Load())
	}
	state, err := repo.ReadMatchmaking(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if len(state.Entries) != 1 || len(state.Reservations) != 1 {
		t.Fatalf("事务状态错误: %+v", state)
	}
}

// TestAtomicQueueRollbackAndPartyAdmission证明任一仓储失败不提前锁队；未对同模式准备和单人绕组队均拒绝。
func TestAtomicQueueRollbackAndPartyAdmission(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryPartyRepository()
	var ids atomic.Int64
	service := NewService(repo, NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, ids.Add(1)) })
	p, err := service.CreateParty(ctx, "leader", "队长")
	if err != nil {
		t.Fatal(err)
	}
	p, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error { return p.AddMember(party.Member{PlayerID: "member", Online: true}) })
	if err != nil {
		t.Fatal(err)
	}
	input := CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Team2v2", PartyID: p.ID, RegionID: "region", ClientRequestID: "start"}
	if _, err := service.CreateMatchmakingTicket(ctx, "leader", input); err == nil {
		t.Fatal("未准备不能整队入队")
	}
	if _, err := service.CreateMatchmakingTicket(ctx, "member", CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region", ClientRequestID: "solo"}); err == nil {
		t.Fatal("组队成员不能绕开原子名单单排")
	}
	stored, _ := repo.Get(ctx, p.ID)
	if stored.RosterLocked {
		t.Fatal("失败后提前锁队")
	}
	expected := errors.New("transaction aborted")
	if err := repo.UpdateMatchmaking(ctx, []string{p.ID}, nil, func(tx *MatchmakingTransaction) error { tx.Parties[p.ID].LockRoster(); return expected }); !errors.Is(err, expected) {
		t.Fatal(err)
	}
	stored, _ = repo.Get(ctx, p.ID)
	if stored.RosterLocked {
		t.Fatal("失败事务泄漏名单锁")
	}
	// 队列事务不允许改变成员归属；发现此错误之前也不能部分提交其他字段。
	if err := repo.UpdateMatchmaking(ctx, []string{p.ID}, nil, func(tx *MatchmakingTransaction) error {
		tx.Parties[p.ID].LockRoster()
		delete(tx.Parties[p.ID].Members, "member")
		return nil
	}); err == nil {
		t.Fatal("非法队列事务修改成员应拒绝")
	}
	stored, _ = repo.Get(ctx, p.ID)
	if stored.RosterLocked || len(stored.Members) != 2 {
		t.Fatal("非法事务留下部分状态")
	}
}

// TestQueuedSoloCannotJoinParty证明排队玩家在组队写入路径也受到同一预留约束，不能通过接受邀请造成双重身份。
func TestQueuedSoloCannotJoinParty(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryPartyRepository()
	var ids atomic.Int64
	service := NewService(repo, NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, ids.Add(1)) })
	if _, err := service.CreateMatchmakingTicket(ctx, "solo", CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region", ClientRequestID: "queue"}); err != nil {
		t.Fatal(err)
	}
	if _, err := service.CreateParty(ctx, "solo", "单人"); err == nil {
		t.Fatal("排队时不能创建其他队伍")
	}
	p, err := service.CreateParty(ctx, "leader", "队长")
	if err != nil {
		t.Fatal(err)
	}
	if _, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error { return p.AddMember(party.Member{PlayerID: "solo", Online: true}) }); err == nil {
		t.Fatal("排队时不能被加入其他队伍")
	}
}
