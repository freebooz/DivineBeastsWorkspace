//go:build productiondeps

// 本回归只使用显式隔离Redis DB15；两个服务实例真实共享控制事实/签票回执，不清库、不替代PG结果测试。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchapi"
	contracts "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/party"
	"divinebeasts/backend/internal/modules/servertransfer"
	"fmt"
	"os"
	"testing"
	"time"
)

// TestRedisArenaRecovery核对同Boot恢复、签票唯一回执、已消费拒绝和未分配取消墓碑；失败不能留下可重新入场意图。
func TestRedisArenaRecovery(t *testing.T) {
	addr := os.Getenv("SSLM_PARTY_TEST_REDIS_ADDR")
	if addr == "" {
		t.Skip("需要显式隔离Redis")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	first, err := Open(ctx, Config{Addrs: []string{addr}, DB: 15, PoolSize: 8, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer first.Close()
	second, err := Open(ctx, Config{Addrs: []string{addr}, DB: 15, PoolSize: 8, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	defer second.Close()
	nonce := fmt.Sprintf("arena-%d", time.Now().UnixNano())
	makeService := func(client *Client) *gameservercontrol.Service {
		transfer := servertransfer.NewServiceWithAllStores([]byte("01234567890123456789012345678901"), time.Now, NewTransferReplayStore(client), NewTransferEpochStore(client), NewTransferIssuanceStore(client))
		svc := gameservercontrol.NewService(gameserver.NewRegistry(), transfer, match.NewResultService(match.NewMemoryResultStore()), time.Now)
		if err := svc.ConfigureArenaLifecycle(NewArenaLifecycleStore(client)); err != nil {
			t.Fatal(err)
		}
		return svc
	}
	svc := makeService(first)
	reg := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: nonce, ServerBootID: nonce + "-boot", ServerRoleID: contracts.RoleMainArena, ExperienceID: contracts.ExperienceMainArenaMain, RegionID: "test", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 10}
	if err := svc.Register(reg); err != nil {
		t.Fatal(err)
	}
	if err := svc.SetReady(reg.GameServerID); err != nil {
		t.Fatal(err)
	}
	input := gameservercontrol.AllocateMainArenaInput{MatchID: nonce, ArenaModeID: contracts.ArenaMode1v1, MapID: "test-only", RegionID: "test", Roster: []contracts.RosterPlayer{{PlayerID: nonce + "-a", CharacterID: nonce + "-char-a", TeamID: "team-a"}, {PlayerID: nonce + "-b", CharacterID: nonce + "-char-b", TeamID: "team-b"}}}
	if _, err := svc.AllocateMainArenaContext(ctx, input); err != nil {
		t.Fatal(err)
	}
	issue := gameservercontrol.IssueTransferInput{TicketID: nonce + "-ticket", GameID: "divine-beasts", PlayerID: input.Roster[0].PlayerID, SessionID: nonce + "-session", DestinationGameServerID: nonce, MatchID: nonce, TTL: time.Minute}
	ticket, err := svc.IssueTransferContext(ctx, issue)
	if err != nil {
		t.Fatal(err)
	}
	recovered := makeService(second)
	if err := recovered.Register(reg); err != nil {
		t.Fatal(err)
	}
	again, err := recovered.IssueTransferContext(ctx, issue)
	if err != nil || again != ticket {
		t.Fatal("跨服务必须返回同一秘密/Epoch", err)
	}
	if _, err := recovered.ValidateTransferContext(ctx, ticket, nonce); err != nil {
		t.Fatal(err)
	}
	if _, err := svc.IssueTransferContext(ctx, issue); err == nil {
		t.Fatal("已消费不能重签同身份")
	}
	presence, err := svc.LookupPlayerPresence(ctx, issue.PlayerID, issue.SessionID)
	if err != nil || presence.TicketID != ticket.TicketID {
		t.Fatal("真实准入必须可跨服务读取", err)
	}
	input.MatchID = nonce + "-cancel"
	if err := svc.CancelArena(ctx, input.MatchID); err != nil {
		t.Fatal(err)
	}
	if err := recovered.FinalizeArena(ctx, input.MatchID); err != nil {
		t.Fatal(err)
	}
	if _, err := recovered.AllocateMainArenaContext(ctx, input); err == nil {
		t.Fatal("跨实例不能恢复已取消分配")
	}

	// 世界返回在独立控制实例恢复相同持久目标；首次目标选择在签票之前，跨进程不能重新BestFit。
	villages := []gameservercontrol.RegisterInput{}
	for _, suffix := range []string{"village-a", "village-b"} {
		v := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: nonce + suffix, ServerBootID: nonce + suffix + "-boot", ServerRoleID: contracts.RoleVillage, ExperienceID: contracts.ExperienceVillageTutorial, RegionID: "test", WorldID: nonce + "-world", PublicEndpoint: "127.0.0.1:17781", ProtocolVersion: 1, Capacity: 2}
		villages = append(villages, v)
		if err := svc.Register(v); err != nil {
			t.Fatal(err)
		}
		if err := svc.SetReady(v.GameServerID); err != nil {
			t.Fatal(err)
		}
	}
	returnInput := gameservercontrol.AllocateWorldTransferInput{World: gameservercontrol.AllocateWorldInput{ExperienceID: contracts.ExperienceVillageTutorial, WorldID: nonce + "-world", RegionID: "test", PlayerSlots: 1}, Transfer: gameservercontrol.IssueTransferInput{TicketID: nonce + "-return", GameID: "divine-beasts", PlayerID: issue.PlayerID, SessionID: issue.SessionID, TTL: time.Minute}}
	returned, err := svc.AllocateWorldTransfer(ctx, returnInput)
	if err != nil {
		t.Fatal(err)
	}
	for _, v := range villages {
		if err := recovered.Register(v); err != nil {
			t.Fatal(err)
		}
		if err := recovered.SetReady(v.GameServerID); err != nil {
			t.Fatal(err)
		}
	}
	sameReturn, err := recovered.AllocateWorldTransfer(ctx, returnInput)
	if err != nil || sameReturn.Ticket != returned.Ticket || sameReturn.Assignment.GameServerID != returned.Assignment.GameServerID {
		t.Fatal("跨进程返回必须恢复同目标同票据", err)
	}
	// 续租只更新内部元数据；两个真实客户端读取相同公开游标，长等待不得自我唤醒。
	partyRepo := NewPartyRepository(first, time.Hour)
	if _, err := partyRepo.MigrateMembershipIndex(ctx, 10000); err != nil {
		t.Fatal(err)
	}
	p := party.New(nonce+"-party", party.Member{PlayerID: nonce + "-leader"})
	if err := partyRepo.Save(ctx, p); err != nil {
		t.Fatal(err)
	}
	observer := matchapi.NewService(NewPartyRepository(second, time.Hour), matchapi.NewMemoryTicketRepository(), func(string) string { return "unused" })
	initial, err := observer.ObserveParty(ctx, p.LeaderPlayerID, "", 0)
	if err != nil {
		t.Fatal(err)
	}
	start := time.Now()
	silent, err := observer.ObserveParty(ctx, p.LeaderPlayerID, initial.Cursor, 40*time.Millisecond)
	if err != nil || silent.Cursor != initial.Cursor || silent.Changed || time.Since(start) < 30*time.Millisecond {
		t.Fatal("真实Redis续租不能触发观察反馈", err)
	}
	defer first.inner.Del(context.Background(), "gameserver:world-transfer:"+returnInput.Transfer.TicketID, "transfer:issued:"+returnInput.Transfer.TicketID, transferReplayKeyPrefix+returnInput.Transfer.TicketID, partyKeyPrefix+p.ID, partyMemberPrefix+p.LeaderPlayerID, partyChangesPrefix+p.LeaderPlayerID)
	// 仅清本测试生成的已知键；不使用FLUSHDB、通配扫描或删除其它会话记录。
	keys := []string{arenaRecordPrefix + nonce, arenaRecordPrefix + input.MatchID, arenaServerIndexPrefix + nonce, playerPresencePrefix + issue.PlayerID, "transfer:issued:" + issue.TicketID, transferReplayKeyPrefix + issue.TicketID}
	if err := first.inner.Del(ctx, keys...).Err(); err != nil {
		t.Fatal(err)
	}
}
