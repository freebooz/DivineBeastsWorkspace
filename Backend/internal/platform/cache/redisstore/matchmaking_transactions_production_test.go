//go:build productiondeps

// 本文件仅使用显式隔离Redis测试地址，不清空数据库；跨两个客户端验证实际WATCH/EXEC与取消回滚。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
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

// 双客户端验证真实目标Party解析、邀请通知、原子锁定及同一候选；去掉Redis目标索引WATCH/读取会失败。
func TestRedisChallengeFixedTeams(t *testing.T) {
	address := os.Getenv("SSLM_PARTY_TEST_REDIS_ADDR")
	if address == "" {
		t.Skip("需要显式隔离Redis地址")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	clients := []*Client{}
	repos := []*PartyRepository{}
	services := []*matchapi.Service{}
	nonce := fmt.Sprintf("challenge-%d", time.Now().UnixNano())
	var ids atomic.Int64
	for i := 0; i < 2; i++ {
		client, err := Open(ctx, Config{Addrs: []string{address}, DB: 15, PoolSize: 16, DialTimeout: time.Second})
		if err != nil {
			t.Fatal(err)
		}
		defer client.Close()
		clients = append(clients, client)
		repo := NewPartyRepository(client, time.Hour)
		repos = append(repos, repo)
		if _, err := repo.MigrateMembershipIndex(ctx, 10000); err != nil {
			t.Fatal(err)
		}
		s := matchapi.NewService(repo, matchapi.NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%s-%d", prefix, nonce, ids.Add(1)) })
		if err := s.ConfigureMatchmaking(matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only", ContentRevision: "test-only", ProjectRuleRevision: 1, HeroCatalogRevision: 1, AllowedRegions: []string{"region"}}); err != nil {
			t.Fatal(err)
		}
		services = append(services, s)
	}
	actor := func(player string) matchmaking.ActorContext {
		return matchmaking.ActorContext{PlayerID: player, SessionID: "s-" + player, CharacterID: "c-" + player, HeroDefinitionID: "Hero.Zodiac.Rat", CharacterRevision: 1}
	}
	leaders := []string{nonce + "-a", nonce + "-b"}
	parties := []*party.Party{}
	for i, leader := range leaders {
		p, err := services[i].CreateParty(ctx, leader, leader)
		if err != nil {
			t.Fatal(err)
		}
		p, err = repos[i].Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
			return p.AddMember(party.Member{PlayerID: leader + "-member", Online: true})
		})
		if err != nil {
			t.Fatal(err)
		}
		parties = append(parties, p)
	}
	cursor, err := repos[1].CurrentPartyCursor(ctx, leaders[1])
	if err != nil {
		t.Fatal(err)
	}
	command := matchmaking.Command{Action: "challenge", ClientRequestID: "invite", ExpectedRevision: parties[0].Revision, PartyID: parties[0].ID, TargetPlayerID: leaders[1] + "-member", ArenaModeID: "Arena.Mode.Team2v2", RegionID: "region"}
	invitation, err := services[0].CommandMatchmaking(ctx, actor(leaders[0]), command)
	if err != nil {
		t.Fatal(err)
	}
	if invitation.Challenge == nil || invitation.Challenge.TargetPlayerID != leaders[1] {
		t.Fatalf("目标Party没有通过真实索引解析: %+v", invitation)
	}
	observed, err := services[1].ObserveMatchmaking(ctx, leaders[1], cursor, 0)
	if err != nil || !observed.Changed || observed.Snapshot.Challenge == nil || !observed.Snapshot.Challenge.CanReply {
		t.Fatalf("接收者未收到持久邀请通知: %+v %v", observed, err)
	}
	for i, p := range parties {
		current, err := repos[i].Get(ctx, p.ID)
		if err != nil || current.RosterLocked {
			t.Fatal("邀请擅自锁定Party", err)
		}
	}
	if _, err := services[1].CommandMatchmaking(ctx, actor(leaders[0]), command); err != nil {
		t.Fatal("跨进程幂等失败", err)
	}
	accepted, err := services[1].CommandMatchmaking(ctx, actor(leaders[1]), matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: invitation.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
	if err != nil {
		t.Fatal(err)
	}
	if accepted.State != matchmaking.PhaseConfirmation || accepted.Challenge != nil || len(accepted.Participants) != 4 {
		t.Fatalf("固定候选未替换邀请: %+v", accepted)
	}
	q, err := repos[0].ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	for _, slot := range accepted.Participants {
		entry := q.Entries[q.Reservations[slot.PlayerID]]
		if entry.CandidateID != accepted.CandidateID || slot.Accepted {
			t.Fatal("双方未占有同一未确认候选")
		}
	}
	for i, p := range parties {
		current, _ := repos[i].Get(ctx, p.ID)
		if !current.RosterLocked {
			t.Fatal("接受未原子锁两队")
		}
	}
	entry := q.Entries[q.Reservations[leaders[0]]]
	if _, err := services[0].CommandMatchmaking(ctx, actor(leaders[0]), matchmaking.Command{Action: "cancel", ClientRequestID: "cancel", TicketID: entry.Ticket.TicketID, ExpectedRevision: entry.Revision}); err != nil {
		t.Fatal(err)
	}
	for i, p := range parties {
		current, _ := repos[i].Get(ctx, p.ID)
		if current.RosterLocked {
			t.Fatal("取消固定候选没有恢复队伍")
		}
	}
	// 第二轮邀请的目标仍能转让队长；结构变化须撤销整轮邀请，不能留下旧成员预留。
	ownerParty, _ := repos[0].Get(ctx, parties[0].ID)
	command.ClientRequestID = "invite-roster-change"
	command.ExpectedRevision = ownerParty.Revision
	changed, err := services[0].CommandMatchmaking(ctx, actor(leaders[0]), command)
	if err != nil {
		t.Fatal(err)
	}
	targetParty, _ := repos[1].Get(ctx, parties[1].ID)
	if _, err := services[1].CommandParty(ctx, leaders[1], party.Command{Action: "transferLeader", ClientRequestID: "transfer", PartyID: targetParty.ID, ExpectedRevision: targetParty.Revision, TargetPlayerID: leaders[1] + "-member"}); err != nil {
		t.Fatalf("待邀请目标被强制冻结: %v", err)
	}
	q, err = repos[0].ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	if q.Challenges[changed.Challenge.ChallengeID].Phase != matchmaking.PhaseCancelled {
		t.Fatal("真实队长变化没有终结邀请")
	}
	for _, player := range q.Challenges[changed.Challenge.ChallengeID].PlayerIDs() {
		if q.Reservations[player] != "" {
			t.Fatal("名单变化留下旧邀请资格")
		}
	}
	// 两个真实Redis客户端并发邀请同一solo，只能成功占有一轮邀请。
	var workers sync.WaitGroup
	var successes atomic.Int64
	target := nonce + "-solo-target"
	for i := 0; i < 12; i++ {
		workers.Add(1)
		go func(i int) {
			defer workers.Done()
			_, err := services[i%2].CommandMatchmaking(ctx, actor(fmt.Sprintf("%s-inviter-%d", nonce, i)), matchmaking.Command{Action: "challenge", ClientRequestID: "race", TargetPlayerID: target, ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
			if err == nil {
				successes.Add(1)
			} else if !strings.HasPrefix(err.Error(), "MATCH_PLAYER_RESERVED:") && !strings.HasPrefix(err.Error(), "MATCH_REVISION_CONFLICT:") {
				t.Errorf("并发邀请异常: %v", err)
			}
		}(i)
	}
	workers.Wait()
	if successes.Load() != 1 {
		t.Fatalf("真实Redis并发目标资格重复: %d", successes.Load())
	}
	view, err := services[1].ReadMatchmakingState(ctx, target)
	if err != nil || view.Challenge == nil {
		t.Fatal("并发邀请不存在", err)
	}
	// 接受与取消也争用同一事务版本，胜出后不会同时出现邀请和候选预留。
	var commits atomic.Int64
	for i, action := range []string{"challengeReply", "challengeCancel"} {
		workers.Add(1)
		go func(i int, action string) {
			defer workers.Done()
			player := target
			if action == "challengeCancel" {
				player = view.Challenge.OwnerPlayerID
			}
			_, err := services[i].CommandMatchmaking(ctx, actor(player), matchmaking.Command{Action: action, ClientRequestID: action, ChallengeID: view.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
			if err == nil {
				commits.Add(1)
			}
		}(i, action)
	}
	workers.Wait()
	if commits.Load() != 1 {
		t.Fatalf("真实Redis接受/取消发生重复提交: %d", commits.Load())
	}
	q, err = repos[0].ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	if ticket := q.Reservations[target]; ticket != "" {
		entry := q.Entries[ticket]
		if entry.Phase != matchmaking.PhaseConfirmation {
			t.Fatal("接受/取消竞争留下幽灵资格")
		}
		ownerEntry := q.Entries[q.Reservations[view.Challenge.OwnerPlayerID]]
		if _, err := services[0].CommandMatchmaking(ctx, actor(view.Challenge.OwnerPlayerID), matchmaking.Command{Action: "cancel", ClientRequestID: "race-cleanup", TicketID: ownerEntry.Ticket.TicketID, ExpectedRevision: ownerEntry.Revision}); err != nil {
			t.Fatal(err)
		}
	}
}
