package match

import (
	"context"
	"testing"
)

func TestSubmitResultIsIdempotentByMatchID(t *testing.T) {
	store := NewMemoryResultStore()
	service := NewResultService(store)
	result := Result{MatchID: "match-1", ArenaModeID: "Arena.Mode.Duel1v1", GameServerID: "arena-1", WinningTeamID: "team-a"}

	first, err := service.Submit(context.Background(), result)
	if err != nil {
		t.Fatal(err)
	}
	second, err := service.Submit(context.Background(), result)
	if err != nil {
		t.Fatal(err)
	}
	if first.ResultID != second.ResultID {
		t.Fatal("重复提交必须返回同一ResultID")
	}
}

func TestSubmitResultPreservesTeamResults(t *testing.T) {
	store := NewMemoryResultStore()
	service := NewResultService(store)
	result := Result{
		MatchID: "match-team", ArenaModeID: "Arena.Mode.Team2v2", GameServerID: "arena-2", WinningTeamID: "team-a",
		Teams: []TeamResult{{TeamID: "team-a", Won: true, Score: 10}, {TeamID: "team-b", Won: false, Score: 4}},
	}
	stored, err := service.Submit(context.Background(), result)
	if err != nil {
		t.Fatal(err)
	}
	if len(stored.Result.Teams) != 2 || stored.Result.Teams[0].TeamID != "team-a" {
		t.Fatalf("TeamResult未完整保存: %+v", stored.Result.Teams)
	}
	stored.Result.Teams[0].Score = 999
	again, _, err := store.Get(context.Background(), "match-team")
	if err != nil {
		t.Fatal(err)
	}
	if again.Result.Teams[0].Score != 10 {
		t.Fatal("StoredResult必须深拷贝Team切片")
	}
}

// TestTransactionalStoreReceivesMatchCompletedEvent（比赛结果事务事件测试）验证结果服务会把Match.Completed交给事务仓储。
func TestTransactionalStoreReceivesMatchCompletedEvent(t *testing.T) {
	store := NewMemoryResultStore()
	service := NewResultService(store)
	result := Result{MatchID: "match-outbox", ArenaModeID: "Arena.Mode.Team3v3", GameServerID: "arena-3", WinningTeamID: "team-b"}
	if _, err := service.Submit(context.Background(), result); err != nil {
		t.Fatal(err)
	}
	event, ok := store.Event("match-completed:match-outbox")
	if !ok {
		t.Fatal("事务仓储必须同时保存Match.Completed事件")
	}
	if event.Topic != "Match.Completed" || event.AggregateID != "match-outbox" || len(event.Payload) == 0 {
		t.Fatalf("事件内容错误: %+v", event)
	}
}
