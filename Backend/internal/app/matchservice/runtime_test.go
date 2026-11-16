// 本文件运行真实队列/控制领域服务的五模式编排回归；测试替身只注入明确网络故障，绝不固定返回分配成功。
package matchservice_test

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/json"
	"errors"
	"fmt"
	"slices"
	"strconv"
	"sync/atomic"
	"testing"
	"time"
)

// interruptedControl第一次在真实签票后丢失响应，验证重试恢复同一凭据且未向任何人提前发布。
type interruptedControl struct {
	*gameservercontrol.Service
	failPlayer string
	lost       bool
	lostReturn bool
}

type candidateProjectionProbe struct {
	matchservice.CandidateStore
	failedPhase  string
	lostAckPhase string
	phases       []string
}

func (p *candidateProjectionProbe) CommitCandidatePhase(ctx context.Context, id, phase, reason string, deadline time.Time) error {
	p.phases = append(p.phases, phase)
	if phase == p.failedPhase {
		p.failedPhase = ""
		return errors.New("测试注入：持久步骤后的候选投影未提交")
	}
	if err := p.CandidateStore.CommitCandidatePhase(ctx, id, phase, reason, deadline); err != nil {
		return err
	}
	if phase == p.lostAckPhase {
		p.lostAckPhase = ""
		return errors.New("测试注入：候选投影已提交后的未知响应")
	}
	return nil
}

func (c *interruptedControl) IssueTransferContext(ctx context.Context, input gameservercontrol.IssueTransferInput) (servertransfer.Ticket, error) {
	value, err := c.Service.IssueTransferContext(ctx, input)
	if err == nil && !c.lost && input.PlayerID == c.failPlayer {
		c.lost = true
		return servertransfer.Ticket{}, errors.New("测试注入：真实签票成功后的未知响应")
	}
	return value, err
}

// AllocateWorldTransfer只在真实返回签票后丢响应，上层用同一稳定意图恢复，不能重分配或重新占容量。
func (c *interruptedControl) AllocateWorldTransfer(ctx context.Context, input gameservercontrol.AllocateWorldTransferInput) (gameservercontrol.WorldTransferResult, error) {
	value, err := c.Service.AllocateWorldTransfer(ctx, input)
	if err == nil && !c.lostReturn {
		c.lostReturn = true
		return gameservercontrol.WorldTransferResult{}, errors.New("测试注入：真实返回签票后的未知响应")
	}
	return value, err
}

// TestRuntimeFiveModesAndSafeReturn覆盖真实报名/逐人确认、未知签票恢复、入场、可信结果及重新分配来源体验。
func TestRuntimeFiveModesAndSafeReturn(t *testing.T) {
	for scenario := 0; scenario < 20; scenario++ {
		size := scenario%5 + 1
		bFault := scenario%10 >= 5
		challenge := scenario >= 10
		stoppedAfterResult := scenario == 0 || scenario == 4
		abortReason := map[int]string{5: "player_disconnected", 9: "spawn_failed"}[scenario]
		t.Run(fmt.Sprintf("%dv%d-fault-%t-challenge-%t", size, size, bFault, challenge), func(t *testing.T) {
			ctx := context.Background()
			var counter atomic.Int64
			nextID := func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, counter.Add(1)) }
			repo := matchapi.NewMemoryPartyRepository()
			queue := matchapi.NewService(repo, matchapi.NewMemoryTicketRepository(), nextID)
			mode := "Arena.Mode.Team" + strconv.Itoa(size) + "v" + strconv.Itoa(size)
			if size == 1 {
				mode = "Arena.Mode.Duel1v1"
			}
			settings := matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only-map", ContentRevision: "test-only-content", ProjectRuleRevision: 1, HeroCatalogRevision: 1, AllowedRegions: []string{"test"}}
			if err := queue.ConfigureMatchmaking(settings); err != nil {
				t.Fatal(err)
			}
			controlNow := time.Now().UTC()
			controlClock := func() time.Time { return controlNow }
			control := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), controlClock), match.NewResultService(match.NewMemoryResultStore()), controlClock)
			if err := control.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
				t.Fatal(err)
			}
			arenaRegistration := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "arena", ServerBootID: "arena-boot", ServerRoleID: gameservercontract.RoleMainArena, ExperienceID: gameservercontract.ExperienceMainArenaMain, RegionID: "test", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 10}
			for _, registration := range []gameservercontrol.RegisterInput{arenaRegistration, {GameID: "divine-beasts", GameServerID: "new-village", ServerBootID: "village-boot", ServerRoleID: gameservercontract.RoleVillage, ExperienceID: gameservercontract.ExperienceVillageTutorial, RegionID: "test", WorldID: "divinebeasts.world.village_tutorial@1", PublicEndpoint: "127.0.0.1:17781", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 20}} {
				if err := control.Register(registration); err != nil {
					t.Fatal(err)
				}
				if err := control.SetReady(registration.GameServerID); err != nil {
					t.Fatal(err)
				}
			}
			actors := []matchmaking.ActorContext{}
			for index := 0; index < size*2; index++ {
				player := fmt.Sprintf("player-%02d", index)
				actor := matchmaking.ActorContext{PlayerID: player, SessionID: "session-" + player, CharacterID: "char-" + player, HeroDefinitionID: "hero-test", CharacterRevision: 1, SourceGameServerID: "old-village-that-no-longer-exists", SourceExperienceID: gameservercontract.ExperienceVillageTutorial}
				if index == 0 {
					actor.CharacterName = "白君"
				}
				actors = append(actors, actor)
				if !challenge {
					if _, err := queue.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "start", ClientRequestID: "start-" + player, ArenaModeID: mode, RegionID: "test"}); err != nil {
						t.Fatal(err)
					}
				}
			}
			if challenge {
				partyID := ""
				revision := int64(0)
				if size > 1 {
					for team := 0; team < 2; team++ {
						p, err := queue.CreateParty(ctx, actors[team*size].PlayerID, "test-leader")
						if err != nil {
							t.Fatal(err)
						}
						p, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(p *party.Party) error {
							for i := 1; i < size; i++ {
								if err := p.AddMember(party.Member{PlayerID: actors[team*size+i].PlayerID, Online: true}); err != nil {
									return err
								}
							}
							return nil
						})
						if err != nil {
							t.Fatal(err)
						}
						if team == 0 {
							partyID = p.ID
							revision = p.Revision
						}
					}
				}
				invite, err := queue.CommandMatchmaking(ctx, actors[0], matchmaking.Command{Action: "challenge", ClientRequestID: "invite", PartyID: partyID, ExpectedRevision: revision, TargetPlayerID: actors[size].PlayerID, ArenaModeID: mode, RegionID: "test"})
				if err != nil {
					t.Fatal(err)
				}
				if invite.Challenge == nil {
					t.Fatal("缺少约战邀请")
				}
				if _, err := queue.CommandMatchmaking(ctx, actors[size], matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept-invite", ChallengeID: invite.Challenge.ChallengeID, ExpectedRevision: invite.Challenge.Revision, Accept: true}); err != nil {
					t.Fatal(err)
				}
			}
			state, err := queue.ReadMatchmakingState(ctx, actors[0].PlayerID)
			if err != nil || state.State != matchmaking.PhaseConfirmation {
				t.Fatal("两队精确人数才形成候选", state.State, err)
			}
			for _, actor := range actors {
				state, err = queue.ReadMatchmakingState(ctx, actor.PlayerID)
				if err != nil {
					t.Fatal(err)
				}
				if _, err = queue.CommandMatchmaking(ctx, actor, matchmaking.Command{Action: "reply", ClientRequestID: "reply-" + actor.PlayerID, CandidateID: state.CandidateID, ExpectedRevision: state.CandidateRevision, Accept: true}); err != nil {
					t.Fatal(err)
				}
			}
			candidate, _, _, err := queue.ReadCandidateContext(ctx, state.CandidateID)
			if err != nil {
				t.Fatal(err)
			}
			operations := matchservice.NewMemoryOperationStore()
			fault := &interruptedControl{Service: control, failPlayer: candidate.Roster[1].PlayerID}
			runtime, err := matchservice.NewRuntime(operations, queue, fault, matchservice.RuntimeConfig{MapID: settings.MapID, ContentRevision: settings.ContentRevision, RuleRevision: 1, HeroCatalogRevision: 1}, nextID)
			if err != nil {
				t.Fatal(err)
			}
			processErr := runtime.ProcessCandidate(ctx, candidate)
			if processErr == nil {
				t.Fatal("首次必须暴露真实签票后的未知响应")
			}
			partial, found, err := operations.GetOperation(ctx, candidate.ID)
			if err != nil || !found || partial.Stage != "signing" || len(partial.Tickets) != 1 {
				t.Fatalf("必须保存已经收到的逐人回执: found=%v stage=%s count=%d readErr=%v stepErr=%v", found, partial.Stage, len(partial.Tickets), err, processErr)
			}
			if _, err = runtime.GetMigration(ctx, actors[0], matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "early"}); err == nil {
				t.Fatal("部分签票不允许任何玩家取票")
			}
			// 构造新运行器实例，证明无需进程内缓存即可恢复；每次真实控制分配仍使用同一MatchID。
			projection := &candidateProjectionProbe{CandidateStore: queue, failedPhase: matchmaking.PhaseTransferReady}
			newWorker := func() *matchservice.Runtime {
				t.Helper()
				worker, err := matchservice.NewRuntime(operations, projection, fault, matchservice.RuntimeConfig{MapID: settings.MapID, ContentRevision: settings.ContentRevision, RuleRevision: 1, HeroCatalogRevision: 1}, nextID)
				if err != nil {
					t.Fatal(err)
				}
				return worker
			}
			runtime = newWorker()
			if err := queue.AttachMatchmakingRuntime(runtime); err != nil {
				t.Fatal(err)
			}
			if err = runtime.ProcessCandidate(ctx, candidate); err == nil {
				t.Fatal("测试必须遇到已持久步骤但投影未提交")
			}
			op, _, _ := operations.GetOperation(ctx, candidate.ID)
			if op.Stage != "committed" || len(op.Tickets) != len(actors) {
				t.Fatal("必须全员提交后发布")
			}
			candidate, _, _, err = queue.ReadCandidateContext(ctx, candidate.ID)
			if err != nil || candidate.Phase != matchmaking.PhaseAllocating {
				t.Fatalf("失败的投影不能发布阶段: %s %v", candidate.Phase, err)
			}
			projection.phases = nil
			runtime = newWorker()
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			if !slices.Equal(projection.phases, []string{matchmaking.PhaseTransferReady}) {
				t.Errorf("持久阶段追平只需一次投影，实际 %v", projection.phases)
			}
			observed, _, _, err := queue.ReadCandidateContext(ctx, candidate.ID)
			if err != nil || observed.Phase != matchmaking.PhaseTransferReady {
				t.Fatalf("下一轮必须看见已持久投影: %s %v", observed.Phase, err)
			}
			projection.phases = nil
			if err = runtime.ProcessCandidate(ctx, observed); err != nil {
				t.Fatal(err)
			}
			if len(projection.phases) != 0 {
				t.Errorf("已观察到的未变阶段不应重发，实际 %v", projection.phases)
			}
			assignment, found := control.GetAssignment("arena")
			if !found {
				t.Fatal("缺少真实控制面的比赛名单")
			}
			serverAssignment, _ := control.GetServerAssignment("arena")
			for _, row := range assignment.Roster {
				want := ""
				if row.PlayerID == "player-00" {
					want = "白君"
				}
				if row.CharacterName != want {
					t.Errorf("控制面名单角色名 = %q，应为 %q", row.CharacterName, want)
				}
			}
			if scenario == 0 || scenario == 4 {
				// 另一工作者持有真实操作租约时，全员仍可并发读取已提交且尚未消费的首次票。
				lease, err := operations.ClaimOperation(ctx, op, "race-worker", time.Now(), 20*time.Second)
				if err != nil {
					t.Fatal(err)
				}
				start := make(chan struct{})
				results := make(chan error, len(actors))
				for _, actor := range actors {
					go func() {
						<-start
						value, err := queue.GetMatchmakingMigration(ctx, actor, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "concurrent"})
						if err == nil && value.TicketID != op.TicketIDs[actor.PlayerID] {
							err = errors.New("首次只读请求返回了错误的票据")
						}
						results <- err
					}()
				}
				close(start)
				for range actors {
					if err := <-results; err != nil {
						t.Errorf("首次取票不应争用写租约: %v", err)
					}
				}
				changedSession := actors[0]
				changedSession.SessionID = "new-session"
				if _, err := queue.GetMatchmakingMigration(ctx, changedSession, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "fresh"}); !errors.Is(err, matchservice.ErrOperationBusy) {
					t.Errorf("新签票必须仍受写租约保护: %v", err)
				}
				if err := operations.ReleaseOperation(ctx, op.ID, lease.LeaseOwner, lease.Fence); err != nil {
					t.Fatal(err)
				}
			}
			for _, actor := range actors {
				migration, err := runtime.GetMigration(ctx, actor, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "entry"})
				if err != nil {
					t.Fatal(err)
				}
				var ticket servertransfer.Ticket
				if err = json.Unmarshal([]byte(migration.TransferTicket), &ticket); err != nil {
					t.Fatal(err)
				}
				if _, err = control.ValidateTransferContext(ctx, ticket, "arena"); err != nil {
					t.Fatal(err)
				}
			}
			if err = control.Heartbeat("arena", len(actors), gameserver.StatusActive); err != nil {
				t.Fatal(err)
			}
			// 测试替身不负责重连成功；这里验证新认证会话获得真正可消费的新票，旧票仍然一次性。
			actors[0].SessionID = "reconnected-" + actors[0].SessionID
			reconnect, err := runtime.GetMigration(ctx, actors[0], matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "reconnect-1"})
			if err != nil {
				t.Fatal("已消费入场不能阻止同角色重新认证恢复", err)
			}
			same, err := runtime.GetMigration(ctx, actors[0], matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "arena", ClientRequestID: "reconnect-1"})
			if err != nil || same.TransferTicket != reconnect.TransferTicket {
				t.Fatal("未知响应恢复必须取回同一新票", err)
			}
			var reconnectTicket servertransfer.Ticket
			_ = json.Unmarshal([]byte(reconnect.TransferTicket), &reconnectTicket)
			if reconnectTicket.TicketID == op.TicketIDs[actors[0].PlayerID] {
				t.Fatal("不能重用已消费入场身份")
			}
			if _, err = control.ValidateTransferContext(ctx, reconnectTicket, "arena"); err != nil {
				t.Fatal(err)
			}
			if _, err = control.ValidateTransferContext(ctx, reconnectTicket, "arena"); err == nil {
				t.Fatal("新票也必须只能消费一次")
			}
			// 新会话在committed汇合前消费恢复票，不能因仍比较旧会话而永远停在入场阶段。
			projection.lostAckPhase = matchmaking.PhaseInMatch
			if err = runtime.ProcessCandidate(ctx, observed); err == nil {
				t.Fatal("测试必须遇到局内投影已提交但响应未知")
			}
			candidate, _, _, err = queue.ReadCandidateContext(ctx, candidate.ID)
			if err != nil || candidate.Phase != matchmaking.PhaseInMatch {
				t.Fatalf("全部真实准入后才持久投影局内: %s %v", candidate.Phase, err)
			}
			projection.phases = nil
			runtime = newWorker()
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil || len(projection.phases) != 0 {
				t.Fatalf("替换工作器读取已提交的局内投影后不应重发: %v %v", projection.phases, err)
			}
			result := match.Result{EndReason: "Elimination", EndRevision: 1, MatchID: op.MatchID, ArenaModeID: mode, GameServerID: "arena", StartedAt: time.Now().Add(-time.Minute), EndedAt: time.Now(), WinningTeamID: "team-a", Teams: []match.TeamResult{{TeamID: "team-a", Won: true}, {TeamID: "team-b"}}}
			for _, slot := range op.Assignment.Roster {
				result.Players = append(result.Players, match.PlayerResult{PlayerID: slot.PlayerID, TeamID: slot.TeamID, CharacterID: slot.CharacterID})
			}
			if abortReason != "" {
				if abortReason == "player_disconnected" {
					if err = control.Heartbeat("arena", 0, gameserver.StatusActive); err != nil {
						t.Fatal(err)
					}
				}
				if err = control.ReportArenaStartAbort(ctx, gameservercontrol.ArenaStartAbortInput{MatchID: op.MatchID, GameServerID: "arena", ServerBootID: arenaRegistration.ServerBootID, Reason: abortReason}); err != nil {
					t.Fatal("实际专服开战失败报告未接受", err)
				}
			} else if bFault {
				if err = control.CancelArena(ctx, op.MatchID); err != nil {
					t.Fatal(err)
				}
			} else if _, err = control.SubmitMatchResult(ctx, result); err != nil {
				t.Fatal(err)
			}
			if stoppedAfterResult {
				arenaRegistration.ServerBootID = "restarted-arena-boot"
				if err := control.Register(arenaRegistration); err != nil {
					t.Fatal(err)
				}
			}
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			if abortReason != "" {
				state, err := queue.ReadMatchmakingState(ctx, actors[0].PlayerID)
				if err != nil || state.State != matchmaking.PhaseReturning || state.ReasonCode != "MATCH_START_ABORTED" {
					t.Fatalf("健康实例报告开战失败应立即进入真实返程: %s %s %v", state.State, state.ReasonCode, err)
				}
				aborted, _, err := operations.GetOperation(ctx, candidate.ID)
				if err != nil || aborted.Result != nil {
					t.Fatalf("开战失败不应产生比赛结果: %v", err)
				}
			}
			if stoppedAfterResult {
				state, err := queue.ReadMatchmakingState(ctx, actors[0].PlayerID)
				if err != nil || state.State != matchmaking.PhaseCompleted || state.CandidateID != candidate.ID {
					t.Fatalf("已结算实例失效后应完成释放并保留候选身份: %s %v", state.State, err)
				}
				wrongCharacter := actors[0]
				wrongCharacter.CharacterID = "different-character"
				if _, err := queue.GetMatchmakingMigration(ctx, wrongCharacter, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "return", ClientRequestID: "wrong-character"}); err == nil {
					t.Fatal("完成阶段不能允许另一角色签返回票")
				}
				if _, err := queue.GetMatchmakingMigration(ctx, actors[0], matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "return", ClientRequestID: "wrong-assignment", SourceAssignmentID: "match:different"}); err == nil {
					t.Fatal("完成阶段不能使用另一局旧分配签返回票")
				}
			}
			for _, actor := range actors {
				request := matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "return", ClientRequestID: "return-1", SourceAssignmentID: serverAssignment.AssignmentID}
				migration, err := queue.GetMatchmakingMigration(ctx, actor, request)
				if err != nil && !fault.lostReturn {
					t.Fatal(err)
				}
				if err != nil {
					migration, err = queue.GetMatchmakingMigration(ctx, actor, request)
				}
				if err != nil {
					t.Fatal("返回未知响应必须恢复原稳定意图", err)
				}
				if migration.GameServerID != "new-village" || migration.ExperienceID != actor.SourceExperienceID {
					t.Fatal("必须重分配来源体验，不能复用失效地址")
				}
				var ticket servertransfer.Ticket
				_ = json.Unmarshal([]byte(migration.TransferTicket), &ticket)
				if _, err = control.ValidateTransferContext(ctx, ticket, migration.GameServerID); err != nil {
					t.Fatal(err)
				}
				if stoppedAfterResult {
					request.ClientRequestID = "already-returned"
					if _, err := queue.GetMatchmakingMigration(ctx, actor, request); err == nil {
						t.Fatal("已返回的角色不能再次签旧竞技候选的返程票")
					}
				}
			}
			controlNow = controlNow.Add(time.Second)
			if err = control.Heartbeat("arena", 0, gameserver.StatusActive); err != nil {
				t.Fatal(err)
			}
			if err = control.ReportServerDrain("arena", arenaRegistration.ServerBootID); err != nil {
				t.Fatal(err)
			}
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			state, _ = queue.ReadMatchmakingState(ctx, actors[0].PlayerID)
			if state.State != matchmaking.PhaseCompleted {
				t.Fatal("真实迁出后必须释放预留", state.State)
			}
		})
	}
}
