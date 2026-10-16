// 本文件验证安全返回前不回收竞技实例、真实准入位置和同Boot恢复；内存仓储只用于应用规则回归。
package gameservercontrol

import (
	"context"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
	"testing"
	"time"
)

// lifecycleControlFixture显式装配新生命周期，不改变旧Foundation兼容测试的默认结束策略。
func lifecycleControlFixture(t *testing.T) (*Service, *MemoryArenaLifecycleStore, RegisterInput, AllocateMainArenaInput) {
	t.Helper()
	now := time.Now().UTC()
	store := NewMemoryArenaLifecycleStore()
	service := NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return time.Now().UTC() }), match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return time.Now().UTC() })
	if err := service.ConfigureArenaLifecycle(store); err != nil {
		t.Fatal(err)
	}
	registration := RegisterInput{GameID: "divine-beasts", GameServerID: "arena-safe", ServerBootID: "boot-safe", ServerRoleID: gameservercontract.RoleMainArena, ExperienceID: gameservercontract.ExperienceMainArenaMain, RegionID: "region", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 10}
	if err := service.Register(registration); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	input := AllocateMainArenaInput{MatchID: "safe-match", ArenaModeID: gameservercontract.ArenaMode1v1, MapID: "test-only-arena", RegionID: "region", Roster: []gameservercontract.RosterPlayer{{PlayerID: "one", TeamID: "team-a", CharacterID: "character-one"}, {PlayerID: "two", TeamID: "team-b", CharacterID: "character-two"}}}
	if _, err := service.AllocateMainArena(input); err != nil {
		t.Fatal(err)
	}
	_ = now
	return service, store, registration, input
}

// TestArenaResultWaitsForSafeReturn证明可信结果只推进结果阶段，仍有实际玩家时不能回收、重新分配或再签入场票。
func TestArenaResultWaitsForSafeReturn(t *testing.T) {
	service, store, registration, input := lifecycleControlFixture(t)
	ctx := context.Background()
	if err := service.Heartbeat(registration.GameServerID, 2, gameserver.StatusActive); err != nil {
		t.Fatal(err)
	}
	for _, player := range []string{"one", "two"} {
		ticket, err := service.IssueTransfer(IssueTransferInput{TicketID: "enter-" + player, GameID: "divine-beasts", PlayerID: player, SessionID: "session-" + player, DestinationGameServerID: registration.GameServerID, MatchID: input.MatchID, TTL: time.Minute})
		if err != nil {
			t.Fatal(err)
		}
		if _, err := service.ValidateTransfer(ticket, registration.GameServerID); err != nil {
			t.Fatal(err)
		}
		presence, err := service.LookupPlayerPresence(ctx, player, "session-"+player)
		if err != nil || presence.GameServerID != registration.GameServerID {
			t.Fatalf("准入位置未保存: %v", err)
		}
	}
	result := match.Result{MatchID: input.MatchID, ArenaModeID: input.ArenaModeID, GameServerID: registration.GameServerID, StartedAt: time.Now().Add(-time.Minute).UTC(), EndedAt: time.Now().UTC(), WinningTeamID: "team-a", Teams: []match.TeamResult{{TeamID: "team-a", Won: true}, {TeamID: "team-b"}}, Players: []match.PlayerResult{{PlayerID: "one", TeamID: "team-a", CharacterID: "character-one"}, {PlayerID: "two", TeamID: "team-b", CharacterID: "character-two"}}}
	if _, err := service.SubmitMatchResult(ctx, result); err != nil {
		t.Fatal(err)
	}
	if _, found := service.GetAssignment(registration.GameServerID); !found {
		t.Fatal("结果提交后提前删除仍有玩家的分配")
	}
	if err := service.FinalizeArena(ctx, input.MatchID); err == nil {
		t.Fatal("仍有玩家时不能回收实例")
	}
	if _, err := service.IssueTransfer(IssueTransferInput{TicketID: "late-entry", GameID: "divine-beasts", PlayerID: "one", SessionID: "session-one", DestinationGameServerID: registration.GameServerID, MatchID: input.MatchID, TTL: time.Minute}); err == nil {
		t.Fatal("结果终态不能继续签入场")
	}
	// 服务器真实报告无玩家后才允许回收；不把客户端关闭结果窗口当作已迁出。
	if err := service.Heartbeat(registration.GameServerID, 0, gameserver.StatusActive); err != nil {
		t.Fatal(err)
	}
	if err := service.FinalizeArena(ctx, input.MatchID); err != nil {
		t.Fatal(err)
	}
	if _, found := service.GetAssignment(registration.GameServerID); found {
		t.Fatal("回收后仍保留活动分配")
	}
	record, found, err := store.GetArenaByMatch(ctx, input.MatchID)
	if err != nil || !found || record.State != "released" || record.Result == nil {
		t.Fatalf("回收丢失可信结果: %v", err)
	}
}

// TestArenaControlSameBootRecovery证明控制面重启后同Boot可恢复分配，新Boot不继承原比赛或旧票据。
func TestArenaControlSameBootRecovery(t *testing.T) {
	original, store, registration, input := lifecycleControlFixture(t)
	recovered := NewService(gameserver.NewRegistry(), original.transfer, original.resultService, func() time.Time { return time.Now().UTC() })
	if err := recovered.ConfigureArenaLifecycle(store); err != nil {
		t.Fatal(err)
	}
	if err := recovered.Register(registration); err != nil {
		t.Fatal(err)
	}
	assignment, found := recovered.GetAssignment(registration.GameServerID)
	if !found || assignment.MatchID != input.MatchID {
		t.Fatal("同Boot没有恢复真实分配")
	}
	if err := recovered.SetReady(registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	if _, err := recovered.AllocateMainArena(input); err != nil {
		t.Fatalf("恢复后同比赛重试不应丢失绑定: %v", err)
	}
	registration.ServerBootID = "boot-successor"
	if err := recovered.Register(registration); err != nil {
		t.Fatal(err)
	}
	if _, found := recovered.GetAssignment(registration.GameServerID); found {
		t.Fatal("后继Boot继承了原分配")
	}
	if err := recovered.FinalizeArena(context.Background(), input.MatchID); err != nil {
		t.Fatalf("原进程已消失应只释放旧记录，不回收后继Boot: %v", err)
	}
	server, _ := recovered.registry.Get(registration.GameServerID)
	if server.ServerBootID != "boot-successor" || server.Status == gameserver.StatusDraining {
		t.Fatal("旧比赛回收伤害了后继进程")
	}
}

// TestCancelBeforeAllocationFencesLateRetry验证容量不足/未知响应后的补偿也形成持久终态，迟到分配不能复活比赛。
func TestCancelBeforeAllocationFencesLateRetry(t *testing.T) {
	service, store, _, input := lifecycleControlFixture(t)
	input.MatchID = "not-yet-allocated"
	ctx := context.Background()
	if err := service.CancelArena(ctx, input.MatchID); err != nil {
		t.Fatal("未分配也必须能记录取消", err)
	}
	if err := service.FinalizeArena(ctx, input.MatchID); err != nil {
		t.Fatal("无玩家无实例取消应收敛", err)
	}
	if _, err := service.AllocateMainArenaContext(ctx, input); err == nil {
		t.Fatal("迟到分配不能重激活已取消意图")
	}
	recovered := NewService(gameserver.NewRegistry(), service.transfer, service.resultService, time.Now)
	if err := recovered.ConfigureArenaLifecycle(store); err != nil {
		t.Fatal(err)
	}
	if _, err := recovered.AllocateMainArenaContext(ctx, input); err == nil {
		t.Fatal("重启不能丢失取消栅栏")
	}
}
