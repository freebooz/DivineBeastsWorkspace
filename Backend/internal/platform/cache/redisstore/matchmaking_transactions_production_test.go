//go:build productiondeps

// 本文件仅使用显式隔离Redis测试地址，不清空数据库；跨两个客户端验证实际WATCH/EXEC与取消回滚。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/party"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// TestRedisMatchmakingAtomicReservation验证并发报名只生成一个有效预留；取消事务Context不会留下队伍锁或票据。
func TestRedisMatchmakingAtomicReservation(t *testing.T) {
	address := os.Getenv("SSLM_PARTY_TEST_REDIS_ADDR")
	if address == "" {
		t.Skip("需要显式隔离Redis地址")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	first, err := Open(ctx, Config{Addrs: []string{address}, DB: 15, PoolSize: 16, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer first.Close()
	second, err := Open(ctx, Config{Addrs: []string{address}, DB: 15, PoolSize: 16, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer second.Close()
	repositories := []*PartyRepository{NewPartyRepository(first, time.Hour), NewPartyRepository(second, time.Hour)}
	if _, err := repositories[0].MigrateMembershipIndex(ctx, 10000); err != nil {
		t.Fatal(err)
	}
	nonce := fmt.Sprintf("atomic-%d", time.Now().UnixNano())
	actor := nonce + "-solo"
	var ids atomic.Int64
	services := []*matchapi.Service{}
	for _, repo := range repositories {
		services = append(services, matchapi.NewService(repo, matchapi.NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%s-%d", prefix, nonce, ids.Add(1)) }))
	}
	var workers sync.WaitGroup
	var successes atomic.Int64
	for i := 0; i < 12; i++ {
		workers.Add(1)
		go func(i int) {
			defer workers.Done()
			_, err := services[i%2].CreateMatchmakingTicket(ctx, actor, matchapi.CreateMatchmakingTicketInput{ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region", ClientRequestID: fmt.Sprint(i)})
			if err == nil {
				successes.Add(1)
			} else if !strings.HasPrefix(err.Error(), "MATCH_PLAYER_RESERVED:") && !strings.HasPrefix(err.Error(), "MATCH_REVISION_CONFLICT:") {
				t.Errorf("意外存储错误: %v", err)
			}
		}(i)
	}
	workers.Wait()
	if successes.Load() != 1 {
		t.Fatalf("Redis应只提交一条有效报名，实际%d", successes.Load())
	}
	state, err := repositories[1].ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	ticketID := state.Reservations[actor]
	if ticketID == "" {
		t.Fatal("真实Redis没有玩家预留")
	}
	if _, err := services[1].CreateParty(ctx, actor, "排队玩家"); err == nil {
		t.Fatal("其他Redis客户端绕过排队预留创建队伍")
	}
	before, err := json.Marshal(state)
	if err != nil {
		t.Fatal(err)
	}
	aborted, abort := context.WithCancel(ctx)
	err = repositories[0].UpdateMatchmaking(aborted, nil, []string{actor}, func(tx *matchapi.MatchmakingTransaction) error {
		entry := tx.Queue.Entries[ticketID]
		if err := tx.Queue.Cancel(actor, ticketID, entry.Revision, time.Now()); err != nil {
			return err
		}
		abort()
		return nil
	})
	if err == nil {
		t.Fatal("已取消事务不应提交")
	}
	after, err := repositories[1].ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	payload, _ := json.Marshal(after)
	if string(before) != string(payload) {
		t.Fatal("取消的Redis事务留下了部分状态")
	}
	// 使用正常领域取消清理本测试自己的预留，保留有限终态恢复记录；不删除其他测试数据。
	err = repositories[0].UpdateMatchmaking(ctx, nil, []string{actor}, func(tx *matchapi.MatchmakingTransaction) error {
		entry := tx.Queue.Entries[ticketID]
		return tx.Queue.Cancel(actor, ticketID, entry.Revision, time.Now())
	})
	if err != nil {
		t.Fatal(err)
	}
	p := party.New(nonce+"-party", party.Member{PlayerID: actor})
	if err := repositories[1].Save(ctx, p); err != nil {
		t.Fatalf("释放预留后应恢复组队: %v", err)
	}
}
