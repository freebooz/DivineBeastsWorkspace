// 固定对手约战回归：真实应用命令、同一事务队列和逐人确认，不以两个随机报名模拟约战。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"encoding/json"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

func challengeCommand(t *testing.T, fields map[string]any) matchmaking.Command {
	t.Helper()
	payload, err := json.Marshal(fields)
	if err != nil {
		t.Fatal(err)
	}
	var command matchmaking.Command
	if err := json.Unmarshal(payload, &command); err != nil {
		t.Fatal(err)
	}
	return command
}

func createChallengeParty(t *testing.T, s *Service, repo *MemoryPartyRepository, prefix string, size int) *party.Party {
	t.Helper()
	p, err := s.CreateParty(context.Background(), prefix, prefix)
	if err != nil {
		t.Fatal(err)
	}
	p, err = repo.Update(context.Background(), p.ID, p.Revision, "", "", func(p *party.Party) error {
		for i := 1; i < size; i++ {
			if err := p.AddMember(party.Member{PlayerID: fmt.Sprintf("%s-%d", prefix, i), Online: true}); err != nil {
				return err
			}
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	return p
}

func sendChallenge(t *testing.T, s *Service, owner, target, mode, partyID string, revision int64) matchmaking.PlayerSnapshot {
	t.Helper()
	state, err := s.CommandMatchmaking(context.Background(), lifecycleTestActor(owner), matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: target, ArenaModeID: mode, RegionID: "region", PartyID: partyID, ExpectedRevision: revision})
	if err != nil {
		t.Fatal(err)
	}
	if state.Challenge == nil {
		t.Fatalf("邀请未投影: %+v", state)
	}
	return state
}

func TestChallengeAllFiveFullTeams(t *testing.T) {
	modes := []string{"Arena.Mode.Duel1v1", "Arena.Mode.Team2v2", "Arena.Mode.Team3v3", "Arena.Mode.Team4v4", "Arena.Mode.Team5v5"}
	for size := 2; size <= 5; size++ {
		t.Run(fmt.Sprint(size), func(t *testing.T) {
			s, repo := newLifecycleTestService(t)
			a := createChallengeParty(t, s, repo, "alpha", size)
			b := createChallengeParty(t, s, repo, "bravo", size)
			invitation := sendChallenge(t, s, "alpha", "bravo-1", modes[size-1], a.ID, a.Revision)
			if invitation.Challenge.TargetPlayerID != "bravo" {
				t.Fatal("服务器未把公开成员ID解析成真实队长")
			}
			for _, id := range []string{a.ID, b.ID} {
				p, _ := repo.Get(context.Background(), id)
				if p.RosterLocked || p.ReadyCheckID != "" {
					t.Fatal("未接受的邀请不应冻结队伍或启动准备")
				}
			}
			member, _ := s.ReadMatchmakingState(context.Background(), "bravo-1")
			if member.Challenge == nil || member.Challenge.CanReply {
				t.Fatal("普通队员不能代替队长回复邀请")
			}
			_, err := s.CommandMatchmaking(context.Background(), lifecycleTestActor("bravo-1"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "bad-member", ChallengeID: invitation.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
			if err == nil {
				t.Fatal("非队长接受成功")
			}
			state, err := s.CommandMatchmaking(context.Background(), lifecycleTestActor("bravo"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: invitation.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
			if err != nil {
				t.Fatal(err)
			}
			if state.State != matchmaking.PhaseConfirmation || len(state.Participants) != size*2 {
				t.Fatalf("固定满队候选错误: %+v", state)
			}
			for _, slot := range state.Participants {
				want := "team-a"
				if strings.HasPrefix(slot.PlayerID, "bravo") {
					want = "team-b"
				}
				if slot.TeamID != want || slot.Accepted {
					t.Fatalf("拆队或提前代确认: %+v", slot)
				}
			}
			for _, id := range []string{a.ID, b.ID} {
				p, _ := repo.Get(context.Background(), id)
				if !p.RosterLocked {
					t.Fatal("接受时未原子锁两队")
				}
			}
		})
	}
}

func TestChallengeTerminalsAndRosterChange(t *testing.T) {
	for _, kind := range []string{"decline", "cancel", "expiry", "roster", "leader", "target-created-party"} {
		t.Run(kind, func(t *testing.T) {
			s, repo := newLifecycleTestService(t)
			ctx := context.Background()
			var p *party.Party
			mode := "Arena.Mode.Duel1v1"
			partyID := ""
			revision := int64(0)
			if kind == "roster" || kind == "leader" {
				p = createChallengeParty(t, s, repo, "owner", 2)
				createChallengeParty(t, s, repo, "target", 2)
				partyID = p.ID
				revision = p.Revision
				mode = "Arena.Mode.Team2v2"
			}
			state := sendChallenge(t, s, "owner", "target", mode, partyID, revision)
			id := state.Challenge.ChallengeID
			command := matchmaking.Command{Action: "challengeReply", ClientRequestID: "terminal", ChallengeID: id, ExpectedRevision: 1, Accept: false}
			actor := "target"
			switch kind {
			case "cancel":
				command.Action = "challengeCancel"
				actor = "owner"
			case "expiry":
				if err := repo.UpdateMatchmaking(ctx, nil, nil, func(tx *MatchmakingTransaction) error {
					c := tx.Queue.Challenges[id]
					c.DeadlineUTC = time.Now().Add(-time.Second)
					tx.Queue.Challenges[id] = c
					return nil
				}); err != nil {
					t.Fatal(err)
				}
			case "roster", "leader":
				target, _ := s.GetCurrentParty(ctx, "target")
				action := "leave"
				who := "target-1"
				if kind == "leader" {
					action = "transferLeader"
					who = "target"
				}
				if _, err := s.CommandParty(ctx, who, party.Command{Action: action, PartyID: target.ID, ClientRequestID: "mutate", ExpectedRevision: target.Revision, TargetPlayerID: "target-1"}); err != nil {
					t.Fatalf("邀请不能冻结目标Party: %v", err)
				}
			case "target-created-party":
				if _, err := s.CreateParty(ctx, "target", "target"); err != nil {
					t.Fatalf("未接受邀请不能冻结无队伍目标: %v", err)
				}
			}
			if kind == "decline" || kind == "cancel" {
				if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor(actor), command); err != nil {
					t.Fatal(err)
				}
			} else {
				if err := s.AdvanceMatchmaking(ctx); err != nil {
					t.Fatal(err)
				}
			}
			q, _ := repo.ReadMatchmaking(ctx)
			if len(q.Reservations) != 0 || len(q.Candidates) != 0 || q.Challenges[id].Phase != matchmaking.PhaseCancelled {
				t.Fatalf("终态未清理: %+v", q)
			}
			if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor("target"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "late", ChallengeID: id, ExpectedRevision: 1, Accept: true}); err == nil {
				t.Fatal("迟到接受复活邀请")
			}
		})
	}
}

func TestChallengeBusyIdempotencyAndPrivacy(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	ctx := context.Background()
	state := sendChallenge(t, s, "owner", "target", "Arena.Mode.Duel1v1", "", 0)
	retry := sendChallenge(t, s, "owner", "target", "Arena.Mode.Duel1v1", "", 0)
	if retry.Challenge.ChallengeID != state.Challenge.ChallengeID {
		t.Fatal("重试生成新邀请")
	}
	outsider, _ := s.ReadMatchmakingState(ctx, "outsider")
	if outsider.Challenge != nil {
		t.Fatal("旁观者枚举了邀请")
	}
	for _, command := range []matchmaking.Command{
		{Action: "start", ClientRequestID: "queue", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"},
		{Action: "challenge", ClientRequestID: "other", TargetPlayerID: "third", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"},
		{Action: "challengeReply", ClientRequestID: "forged", ChallengeID: state.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true},
	} {
		player := "target"
		if command.Action == "challengeReply" {
			player = "outsider"
		}
		if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor(player), command); err == nil {
			t.Fatalf("忙碌/越权没有失败: %+v", command)
		}
	}
	_, err := s.CommandMatchmaking(ctx, lifecycleTestActor("owner"), matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: "third", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
	if err == nil || !strings.HasPrefix(err.Error(), "MATCH_IDEMPOTENCY_CONFLICT:") {
		t.Fatalf("同键异参未拒绝: %v", err)
	}
	q, _ := repo.ReadMatchmaking(ctx)
	if len(q.Challenges) != 1 || len(q.Reservations) != 2 {
		t.Fatal("重复提交扩大资格")
	}
}

func TestChallengeConcurrentInviters(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	var workers sync.WaitGroup
	var success atomic.Int64
	for i := 0; i < 12; i++ {
		workers.Add(1)
		go func(i int) {
			defer workers.Done()
			_, err := s.CommandMatchmaking(context.Background(), lifecycleTestActor(fmt.Sprintf("owner-%d", i)), matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: "target", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
			if err == nil {
				success.Add(1)
			}
		}(i)
	}
	workers.Wait()
	q, _ := repo.ReadMatchmaking(context.Background())
	if success.Load() != 1 || len(q.Reservations) != 2 || len(q.Challenges) != 1 {
		t.Fatalf("并发对手资格重复: success=%d reservations=%v", success.Load(), q.Reservations)
	}
}

// 错误地把约战票据排入随机分区会抢走third，本回归固定断言其继续独立搜索。
func TestChallengeDoesNotConsumeRandomOpponent(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	ctx := context.Background()
	third, err := s.CommandMatchmaking(ctx, lifecycleTestActor("third"), matchmaking.Command{Action: "start", ClientRequestID: "random", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
	if err != nil {
		t.Fatal(err)
	}
	invite := sendChallenge(t, s, "owner", "target", "Arena.Mode.Duel1v1", "", 0)
	accepted, err := s.CommandMatchmaking(ctx, lifecycleTestActor("target"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: invite.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
	if err != nil {
		t.Fatal(err)
	}
	for _, slot := range accepted.Participants {
		if slot.PlayerID == "third" {
			t.Fatal("固定邀请与随机票据错误混队")
		}
	}
	q, _ := repo.ReadMatchmaking(ctx)
	if q.Entries[third.TicketID].Phase != matchmaking.PhaseSearching {
		t.Fatal("无关随机票据被约战消费")
	}
	if accepted.DeadlineUTC == nil || time.Until(*accepted.DeadlineUTC) < 29*time.Second || time.Until(*accepted.DeadlineUTC) > 30*time.Second {
		t.Fatal("约战没有沿用30秒全员确认")
	}
	if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor("owner"), matchmaking.Command{Action: "challengeCancel", ClientRequestID: "late-cancel", ChallengeID: invite.Challenge.ChallengeID, ExpectedRevision: 1}); err == nil {
		t.Fatal("旧邀请取消不应取消已形成候选")
	}
}

func TestChallengeRejectsIncompleteAndChangedAuthority(t *testing.T) {
	for _, kind := range []string{"incomplete", "nonleader", "same-party", "stale-revision", "busy-target"} {
		t.Run(kind, func(t *testing.T) {
			s, repo := newLifecycleTestService(t)
			ctx := context.Background()
			p := createChallengeParty(t, s, repo, "owner", 2)
			createChallengeParty(t, s, repo, "target", 2)
			command := matchmaking.Command{Action: "challenge", ClientRequestID: "invite", PartyID: p.ID, ExpectedRevision: p.Revision, TargetPlayerID: "target", ArenaModeID: "Arena.Mode.Team2v2", RegionID: "region"}
			actor := "owner"
			switch kind {
			case "incomplete":
				command.ArenaModeID = "Arena.Mode.Team3v3"
			case "nonleader":
				actor = "owner-1"
			case "same-party":
				command.TargetPlayerID = "owner-1"
			case "stale-revision":
				command.ExpectedRevision--
			case "busy-target":
				target, _ := s.GetCurrentParty(ctx, "target")
				if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor("target"), matchmaking.Command{Action: "start", ClientRequestID: "random", PartyID: target.ID, ExpectedRevision: target.Revision, ArenaModeID: "Arena.Mode.Team2v2", RegionID: "region"}); err != nil {
					t.Fatal(err)
				}
			}
			if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor(actor), command); err == nil {
				t.Fatalf("非法约战未拒绝: %s", kind)
			}
			q, _ := repo.ReadMatchmaking(ctx)
			if len(q.Challenges) != 0 {
				t.Fatal("失败命令留下约战")
			}
		})
	}
}

func TestChallengeAcceptCancelRaceAndSessionReplay(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	ctx := context.Background()
	invitation := sendChallenge(t, s, "owner", "target", "Arena.Mode.Duel1v1", "", 0)
	newSession := lifecycleTestActor("owner")
	newSession.SessionID = "another-session"
	if _, err := s.CommandMatchmaking(ctx, newSession, matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: "target", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"}); err == nil {
		t.Fatal("新认证代次复用了旧请求回执")
	}
	if _, err := s.CommandMatchmaking(ctx, newSession, matchmaking.Command{Action: "challengeCancel", ClientRequestID: "new-session-cancel", ChallengeID: invitation.Challenge.ChallengeID, ExpectedRevision: 1}); err == nil {
		t.Fatal("新会话复用了旧邀请写资格")
	}
	var workers sync.WaitGroup
	var succeeded atomic.Int64
	for _, action := range []string{"challengeReply", "challengeCancel"} {
		workers.Add(1)
		go func(action string) {
			defer workers.Done()
			player := "target"
			if action == "challengeCancel" {
				player = "owner"
			}
			_, err := s.CommandMatchmaking(ctx, lifecycleTestActor(player), matchmaking.Command{Action: action, ClientRequestID: action, ChallengeID: invitation.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true})
			if err == nil {
				succeeded.Add(1)
			}
		}(action)
	}
	workers.Wait()
	q, _ := repo.ReadMatchmaking(ctx)
	if succeeded.Load() != 1 {
		t.Fatalf("接受与取消必须只有一个事务提交: %d", succeeded.Load())
	}
	if len(q.Candidates) == 0 {
		if len(q.Reservations) != 0 {
			t.Fatal("取消胜出后遗留预留")
		}
	} else {
		if len(q.Candidates) != 1 || len(q.Reservations) != 2 {
			t.Fatal("接受胜出后生成不完整候选")
		}
	}
}

// 接受邀请后若允许换认证代次覆盖两位队长上下文，旧邀请会被新登录复活；最终确认必须重新发起约战。
func TestChallengeFinalReplyRejectsNewLeaderSession(t *testing.T) {
	s, _ := newLifecycleTestService(t)
	ctx := context.Background()
	invite := sendChallenge(t, s, "owner", "target", "Arena.Mode.Duel1v1", "", 0)
	if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor("target"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: invite.Challenge.ChallengeID, ExpectedRevision: 1, Accept: true}); err != nil {
		t.Fatal(err)
	}
	for _, player := range []string{"owner", "target"} {
		state, _ := s.ReadMatchmakingState(ctx, player)
		actor := lifecycleTestActor(player)
		actor.SessionID = "new-" + player
		if _, err := s.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "reply", ClientRequestID: "new-session-final", CandidateID: state.CandidateID, ExpectedRevision: state.CandidateRevision, Accept: true}); err == nil {
			t.Fatalf("%s新会话覆盖了邀请已同意的上下文", player)
		}
	}
}

// 缺少约战分支、误排随机队列或把接受邀请当成全员确认都会使本回归失败。
func TestChallengeFixedSoloCandidate(t *testing.T) {
	s, repo := newLifecycleTestService(t)
	ctx := context.Background()
	invite, err := s.CommandMatchmaking(ctx, lifecycleTestActor("owner"), challengeCommand(t, map[string]any{
		"action": "challenge", "clientRequestId": "invite", "expectedRevision": "0", "targetPlayerId": "target", "arenaModeId": "Arena.Mode.Duel1v1", "regionId": "region",
	}))
	if err != nil {
		t.Fatalf("约战邀请必须由现有命令受理: %v", err)
	}
	payload, _ := json.Marshal(invite)
	var view struct {
		Challenge struct {
			ID       string `json:"challengeId"`
			Revision string `json:"revision"`
		} `json:"challenge"`
	}
	if err := json.Unmarshal(payload, &view); err != nil {
		t.Fatal(err)
	}
	if invite.State != "challengeInvitation" || view.Challenge.ID == "" {
		t.Fatalf("邀请快照缺失: %s", payload)
	}
	q, _ := repo.ReadMatchmaking(ctx)
	if len(q.Candidates) != 0 || len(q.Entries) != 0 {
		t.Fatal("未接受邀请不能生成报名或候选")
	}
	accepted, err := s.CommandMatchmaking(ctx, lifecycleTestActor("target"), challengeCommand(t, map[string]any{
		"action": "challengeReply", "clientRequestId": "accept", "expectedRevision": view.Challenge.Revision, "challengeId": view.Challenge.ID, "accept": true,
	}))
	if err != nil {
		t.Fatal(err)
	}
	if accepted.State != matchmaking.PhaseConfirmation || len(accepted.Participants) != 2 || accepted.CandidateID == "" {
		t.Fatalf("接受后应进入两名真人确认: %+v", accepted)
	}
	for _, slot := range accepted.Participants {
		if slot.Accepted {
			t.Fatal("接受邀请不能替代本人最终确认")
		}
	}
	q, _ = repo.ReadMatchmaking(ctx)
	if len(q.Candidates) != 1 || len(q.Reservations) != 2 {
		t.Fatal("固定候选或唯一预留丢失")
	}
	for _, player := range []string{"owner", "target"} {
		state, _ := s.ReadMatchmakingState(ctx, player)
		if _, err := s.CommandMatchmaking(ctx, lifecycleTestActor(player), matchmaking.Command{Action: "reply", ClientRequestID: "final", CandidateID: state.CandidateID, ExpectedRevision: state.CandidateRevision, Accept: true}); err != nil {
			t.Fatal(err)
		}
	}
	final, _ := s.ReadMatchmakingState(ctx, "owner")
	if final.State != matchmaking.PhaseAllocating {
		t.Fatalf("必须复用既有分配阶段: %+v", final)
	}
}
