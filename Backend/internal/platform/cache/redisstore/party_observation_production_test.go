//go:build productiondeps

// 本文件只在显式提供专用Redis测试地址时运行，不清空数据库；测试身份随机隔离，不能连接生产库做验收。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/party"
	"fmt"
	"os"
	"testing"
	"time"
)

// TestRedisPartyObservation跨两个真实Redis客户端检查提交唤醒、窗口恢复与取消的有界退出。
func TestRedisPartyObservation(t *testing.T) {
	address := os.Getenv("SSLM_PARTY_TEST_REDIS_ADDR")
	if address == "" {
		t.Skip("需要显式专用Redis测试地址")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	client, err := Open(ctx, Config{Addrs: []string{address}, DB: 15, PoolSize: 8, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	repo := NewPartyRepository(client, time.Hour)
	if _, err := repo.MigrateMembershipIndex(ctx, 1000); err != nil {
		t.Fatal(err)
	}
	client2, err := Open(ctx, Config{Addrs: []string{address}, DB: 15, PoolSize: 8, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer client2.Close()
	repo2 := NewPartyRepository(client2, time.Hour)
	id := fmt.Sprintf("party-observe-%d", time.Now().UnixNano())
	actor := id + "-guest"
	leader := id + "-leader"
	p := party.New(id, party.Member{PlayerID: leader})
	if err := repo.Save(ctx, p); err != nil {
		t.Fatal(err)
	}
	observer := matchapi.NewService(repo2, matchapi.NewMemoryTicketRepository(), func(string) string { return "unused" })
	initial, err := observer.ObserveParty(ctx, actor, "", 0)
	if err != nil {
		t.Fatal(err)
	}
	waitctx, stop := context.WithCancel(ctx)
	received := make(chan error, 1)
	go func() {
		_, err := observer.ObserveParty(waitctx, actor, initial.Cursor, 5*time.Second)
		received <- err
	}()
	waitForPartySubscription(t, ctx, client, actor)
	stop()
	select {
	case err := <-received:
		if err == nil {
			t.Fatal("取消等待不能成功")
		}
	case <-time.After(time.Second):
		t.Fatal("Redis观察取消没有及时释放")
	}
	next := make(chan party.Observation, 1)
	go func() {
		view, err := observer.ObserveParty(ctx, actor, initial.Cursor, 2*time.Second)
		if err != nil {
			received <- err
			return
		}
		next <- view
	}()
	waitForPartySubscription(t, ctx, client, actor)
	_, err = repo.Update(ctx, id, p.Revision, "", "", func(value *party.Party) error { return value.Invite(leader, "invite", actor, time.Now()) })
	if err != nil {
		t.Fatal(err)
	}
	select {
	case view := <-next:
		if len(view.Invitations) != 1 || view.Cursor == initial.Cursor {
			t.Fatalf("Redis提交未产生完整通知: %+v", view)
		}
	case err := <-received:
		t.Fatal(err)
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	recovered, err := observer.ObserveParty(ctx, actor, "1-0", 0)
	if err != nil || !recovered.ResyncRequired {
		t.Fatalf("过期游标未重同步: %v %+v", err, recovered)
	}
}

// waitForPartySubscription只为隔离集成测试确认真实阻塞连接已建立，避免把“提交前即返回”误当成唤醒验证。
func waitForPartySubscription(t *testing.T, ctx context.Context, client *Client, actor string) {
	t.Helper()
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		counts, err := client.inner.PubSubNumSub(ctx, partyChangesPrefix+actor).Result()
		if err != nil {
			t.Fatal(err)
		}
		if counts[partyChangesPrefix+actor] > 0 {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatal("专用Redis未建立真实事件等待订阅")
}
