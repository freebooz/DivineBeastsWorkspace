//go:build productiondeps

package redisstore

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"reflect"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"github.com/redis/go-redis/v9"
)

// matchmakingScopedReadHook只计算依赖队伍和成员索引的Redis往返，排除模式门禁与全局状态键。
type matchmakingScopedReadHook struct{ trips atomic.Int64 }

func (*matchmakingScopedReadHook) DialHook(next redis.DialHook) redis.DialHook { return next }

func (h *matchmakingScopedReadHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, command redis.Cmder) error {
		if isMatchmakingScopedRead(command) {
			h.trips.Add(1)
		}
		return next(ctx, command)
	}
}

func (h *matchmakingScopedReadHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return func(ctx context.Context, commands []redis.Cmder) error {
		for _, command := range commands {
			if isMatchmakingScopedRead(command) {
				h.trips.Add(1)
				break
			}
		}
		return next(ctx, commands)
	}
}

func isMatchmakingScopedRead(command redis.Cmder) bool {
	if command.Name() != "watch" && command.Name() != "get" {
		return false
	}
	for _, arg := range command.Args()[1:] {
		key, ok := arg.(string)
		if ok && (strings.HasPrefix(key, partyKeyPrefix) || strings.HasPrefix(key, partyMemberPrefix)) {
			return true
		}
	}
	return false
}

// prepareMatchmakingReadFixture只在显式提供的空隔离Redis DB内布置生产格式的5人队票据和索引。
func prepareMatchmakingReadFixture(t testing.TB, db, ticketCount int) *Client {
	t.Helper()
	address := os.Getenv("SSLM_PARTY_TEST_REDIS_ADDR")
	if address == "" {
		t.Skip("需要显式隔离Redis地址")
	}
	host, _, err := net.SplitHostPort(address)
	if err != nil || !net.ParseIP(host).IsLoopback() {
		t.Fatal("性能夹具只能连接显式回环Redis")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	client, err := Open(ctx, Config{Addrs: []string{address}, DB: db, PoolSize: 8, DialTimeout: time.Second})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = client.Close() })
	if count, err := client.inner.DBSize(ctx).Result(); err != nil || count != 0 {
		t.Fatalf("性能夹具要求空隔离Redis DB %d，读取到 %d 个键，错误 %v", db, count, err)
	}
	if _, err := NewPartyRepository(client, time.Hour).MigrateMembershipIndex(ctx, 1000); err != nil {
		t.Fatal(err)
	}
	queue := matchmaking.NewQueueState()
	now := time.Now().UTC()
	_, err = client.inner.Pipelined(ctx, func(pipe redis.Pipeliner) error {
		for index := 0; index < ticketCount; index++ {
			id := fmt.Sprintf("read-scope-%04d", index)
			leader := id + "-0"
			members := []string{leader}
			team := party.New(id, party.Member{PlayerID: leader})
			for member := 1; member < party.MaxMembers; member++ {
				player := fmt.Sprintf("%s-%d", id, member)
				if err := team.AddMember(party.Member{PlayerID: player}); err != nil {
					return err
				}
				members = append(members, player)
			}
			team.LockRoster()
			payload, err := json.Marshal(team)
			if err != nil {
				return err
			}
			pipe.Set(ctx, partyKeyPrefix+id, payload, time.Hour)
			for _, player := range members {
				pipe.Set(ctx, partyMemberPrefix+player, id, time.Hour)
			}
			ticket, err := matchmaking.NewTicket(matchmaking.CreateTicketRequest{TicketID: id, GameID: "divine-beasts", ArenaModeID: "Arena.Mode.Team5v5", PartyID: id, PartyMemberIDs: members, Region: "test"})
			if err != nil {
				return err
			}
			if err := queue.Enqueue(matchmaking.QueueEntry{Ticket: ticket, OwnerPlayerID: leader, Revision: 1, Phase: matchmaking.PhaseSearching}, now); err != nil {
				return err
			}
		}
		payload, err := json.Marshal(queue)
		if err != nil {
			return err
		}
		pipe.Set(ctx, matchmakingStateKey, payload, 0)
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	return client
}

func TestRedisMatchmakingBatchesWatchedReads(t *testing.T) {
	const ticketCount = 10
	client := prepareMatchmakingReadFixture(t, 14, ticketCount)
	repository := NewPartyRepository(client, time.Hour)
	readHook := &matchmakingScopedReadHook{}
	client.inner.AddHook(readHook)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := repository.UpdateMatchmaking(ctx, nil, nil, func(tx *matchapi.MatchmakingTransaction) error {
		if len(tx.Queue.Entries) != ticketCount || len(tx.Parties) != ticketCount || len(tx.PlayerParties) != ticketCount*party.MaxMembers {
			t.Fatal("事务快照遗漏依赖队伍或成员")
		}
		return nil
	}); err != nil {
		t.Fatal(err)
	}
	if trips := readHook.trips.Load(); trips > 6 || trips == 0 {
		t.Errorf("10条5人队票据的依赖Redis往返 = %d，要求1至6", trips)
	}
	const watchedParty = "read-scope-0000"
	attempts := 0
	var updatedPartyRevision int64
	if err := repository.UpdateMatchmaking(ctx, nil, nil, func(tx *matchapi.MatchmakingTransaction) error {
		attempts++
		tx.Queue.Revision++
		team := tx.Parties[watchedParty]
		if attempts == 1 {
			// 独立普通队伍事务在读取后修改受WATCH保护的聚合，首轮EXEC应当放弃。
			updated, err := repository.Update(ctx, watchedParty, team.Revision, "", "", func(next *party.Party) error {
				next.Revision++
				return nil
			})
			if err == nil {
				updatedPartyRevision = updated.Revision
			}
			return err
		}
		if team.Revision != updatedPartyRevision {
			return fmt.Errorf("重试未重读队伍修订: %d / %d", team.Revision, updatedPartyRevision)
		}
		return nil
	}); err != nil || attempts != 2 {
		t.Fatalf("队伍并发写必须触发一次完整重读: attempts=%d err=%v", attempts, err)
	}
	state, err := repository.ReadMatchmaking(ctx)
	if err != nil {
		t.Fatal(err)
	}
	if state.Revision != ticketCount+1 {
		t.Fatalf("已放弃的首轮不能增加持久队列修订: %d", state.Revision)
	}
	if err := client.inner.RPush(ctx, partyMemberPrefix+"zzz-wrong-type", "wrong").Err(); err != nil {
		t.Fatal(err)
	}
	called := false
	err = repository.UpdateMatchmaking(ctx, nil, []string{"aaa-missing", "zzz-wrong-type"}, func(*matchapi.MatchmakingTransaction) error {
		called = true
		return nil
	})
	if err == nil || !strings.Contains(err.Error(), "WRONGTYPE") || called {
		t.Fatalf("批量读取中缺失索引不能掩盖同批类型错误: called=%v err=%v", called, err)
	}
}

func TestRedisMatchmakingScopesObserverCursors(t *testing.T) {
	client := prepareMatchmakingReadFixture(t, 13, 0)
	repository := NewPartyRepository(client, time.Hour)
	var ids atomic.Int64
	service := matchapi.NewService(repository, matchapi.NewMemoryTicketRepository(), func(prefix string) string {
		return fmt.Sprintf("%s-scope-%d", prefix, ids.Add(1))
	})
	if err := service.ConfigureMatchmaking(matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only", ContentRevision: "test-only", ProjectRuleRevision: 1, AllowedRegions: []string{"match", "quiet", "history"}}); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	actor := func(player string) matchmaking.ActorContext {
		return matchmaking.ActorContext{PlayerID: player, SessionID: "session-" + player, CharacterID: "character-" + player, HeroDefinitionID: "Hero.Zodiac.Rat", CharacterRevision: 1}
	}
	start := func(player, region string) matchmaking.PlayerSnapshot {
		t.Helper()
		view, err := service.CommandMatchmaking(ctx, actor(player), matchmaking.Command{Action: "start", ClientRequestID: "start", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: region})
		if err != nil {
			t.Fatal(err)
		}
		return view
	}
	start("match-one", "match")
	matched := start("match-two", "match")
	if matched.State != matchmaking.PhaseConfirmation || matched.CandidateID == "" {
		t.Fatalf("fixture requires a real candidate: %+v", matched)
	}
	start("quiet-active", "quiet")
	terminal := start("quiet-terminal", "history")
	if _, err := service.CommandMatchmaking(ctx, actor("quiet-terminal"), matchmaking.Command{Action: "cancel", ClientRequestID: "cancel", TicketID: terminal.TicketID, ExpectedRevision: terminal.Revision}); err != nil {
		t.Fatal(err)
	}
	observe := func(player, cursor string) matchmaking.Observation {
		t.Helper()
		view, err := service.ObserveMatchmaking(ctx, player, cursor, 0)
		if err != nil {
			t.Fatal(err)
		}
		return view
	}
	quiet := map[string]matchmaking.Observation{
		"quiet-active": observe("quiet-active", ""), "quiet-terminal": observe("quiet-terminal", ""),
	}
	opponent := observe("match-two", "")
	workerCursor, err := repository.CurrentPartyCursor(ctx, matchapi.MatchmakingWorkObserver)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.CommandMatchmaking(ctx, actor("match-one"), matchmaking.Command{Action: "reply", ClientRequestID: "accept", CandidateID: matched.CandidateID, ExpectedRevision: matched.CandidateRevision, Accept: true}); err != nil {
		t.Fatal(err)
	}
	updated := observe("match-two", opponent.Cursor)
	accepted := false
	for _, participant := range updated.Snapshot.Participants {
		accepted = accepted || (participant.PlayerID == "match-one" && participant.Accepted)
	}
	if !updated.Changed || updated.Cursor == opponent.Cursor || updated.Snapshot.State != matchmaking.PhaseConfirmation || !accepted {
		t.Fatalf("候选尚未推进阶段时，对手也应收到真实接受事件: %+v", updated)
	}
	if cursor, err := repository.CurrentPartyCursor(ctx, matchapi.MatchmakingWorkObserver); err != nil || cursor == workerCursor {
		t.Fatalf("匹配工作器未收到持久提交事件: %q %v", cursor, err)
	}
	for player, prior := range quiet {
		current := observe(player, prior.Cursor)
		prior.Snapshot.ServerTimeUTC = current.Snapshot.ServerTimeUTC
		if current.Changed || current.Cursor != prior.Cursor || !reflect.DeepEqual(current.Snapshot, prior.Snapshot) {
			t.Errorf("无关观察者 %s 收到变更或快照发生变化: cursor %s → %s", player, prior.Cursor, current.Cursor)
		}
	}

	// 无候选的终态票据仍读取当前队伍的准备标记，已离队的历史成员也需收到变化。
	const leader, former = "history-leader", "history-former"
	team := party.New("history-party", party.Member{PlayerID: leader})
	for _, player := range []string{former, "history-remaining"} {
		if err := team.AddMember(party.Member{PlayerID: player}); err != nil {
			t.Fatal(err)
		}
	}
	if err := repository.Save(ctx, team); err != nil {
		t.Fatal(err)
	}
	first, err := service.CommandMatchmaking(ctx, actor(leader), matchmaking.Command{Action: "start", ClientRequestID: "history-first", ExpectedRevision: team.Revision, PartyID: team.ID, ArenaModeID: "Arena.Mode.Team3v3", RegionID: "history"})
	if err != nil || first.State != matchmaking.PhaseReadyCheck {
		t.Fatalf("历史票据夹具需要第一次准备: state=%s err=%v", first.State, err)
	}
	if _, err = service.CommandMatchmaking(ctx, actor(leader), matchmaking.Command{Action: "cancel", ClientRequestID: "history-cancel", TicketID: first.TicketID, ExpectedRevision: first.Revision}); err != nil {
		t.Fatal(err)
	}
	team, err = repository.Get(ctx, team.ID)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = service.CommandParty(ctx, former, party.Command{Action: "leave", ClientRequestID: "history-leave", PartyID: team.ID, ExpectedRevision: team.Revision}); err != nil {
		t.Fatal(err)
	}
	team, err = repository.Get(ctx, team.ID)
	if err != nil {
		t.Fatal(err)
	}
	prior := observe(former, "")
	if prior.Snapshot.TicketID != first.TicketID || prior.Snapshot.State != matchmaking.PhaseCancelled || participantReady(prior.Snapshot, leader) {
		t.Fatalf("历史成员应先看到已取消且清空准备的旧票据: %+v", prior.Snapshot)
	}
	if _, err = service.CommandMatchmaking(ctx, actor(leader), matchmaking.Command{Action: "start", ClientRequestID: "history-next", ExpectedRevision: team.Revision, PartyID: team.ID, ArenaModeID: "Arena.Mode.Team2v2", RegionID: "history"}); err != nil {
		t.Fatal(err)
	}
	current := observe(former, prior.Cursor)
	if !current.Changed || current.Cursor == prior.Cursor || current.Snapshot.TicketID != first.TicketID || !participantReady(current.Snapshot, leader) {
		t.Errorf("历史票据快照变化必须推进原成员游标: %+v", current)
	}
}

func participantReady(snapshot matchmaking.PlayerSnapshot, player string) bool {
	for _, participant := range snapshot.Participants {
		if participant.PlayerID == player {
			return participant.Ready
		}
	}
	return false
}

// BenchmarkRedisMatchmakingScopedReads测真实Redis的完整匹配快照空变更读取和校验，不执行候选分配。
func BenchmarkRedisMatchmakingScopedReads(b *testing.B) {
	for dbOffset, size := range []int{10, 100, 1000} {
		client := prepareMatchmakingReadFixture(b, 10+dbOffset, size)
		repository := NewPartyRepository(client, time.Hour)
		readHook := &matchmakingScopedReadHook{}
		client.inner.AddHook(readHook)
		b.Run(fmt.Sprintf("tickets_%d", size), func(b *testing.B) {
			readHook.trips.Store(0)
			b.ReportAllocs()
			for i := 0; i < b.N; i++ {
				if err := repository.UpdateMatchmaking(context.Background(), nil, nil, func(*matchapi.MatchmakingTransaction) error { return nil }); err != nil {
					b.Fatal(err)
				}
			}
			b.ReportMetric(float64(readHook.trips.Load())/float64(b.N), "scope-rtt/op")
		})
	}
}
