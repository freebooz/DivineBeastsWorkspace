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
	"strconv"
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
		t.Run(fmt.Sprintf("%dv%d-fault-%t-challenge-%t", size, size, bFault, challenge), func(t *testing.T) {
			ctx := context.Background()
			counter := 0
			nextID := func(prefix string) string { counter++; return fmt.Sprintf("%s-%d", prefix, counter) }
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
			control := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return time.Now().UTC() }), match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return time.Now().UTC() })
			if err := control.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
				t.Fatal(err)
			}
			for _, registration := range []gameservercontrol.RegisterInput{{GameID: "divine-beasts", GameServerID: "arena", ServerBootID: "arena-boot", ServerRoleID: gameservercontract.RoleMainArena, ExperienceID: gameservercontract.ExperienceMainArenaMain, RegionID: "test", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:17782", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 10}, {GameID: "divine-beasts", GameServerID: "new-village", ServerBootID: "village-boot", ServerRoleID: gameservercontract.RoleVillage, ExperienceID: gameservercontract.ExperienceVillageTutorial, RegionID: "test", WorldID: "divinebeasts.world.village_tutorial@1", PublicEndpoint: "127.0.0.1:17781", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 20}} {
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
			runtime, err = matchservice.NewRuntime(operations, queue, fault, matchservice.RuntimeConfig{MapID: settings.MapID, ContentRevision: settings.ContentRevision, RuleRevision: 1, HeroCatalogRevision: 1}, nextID)
			if err != nil {
				t.Fatal(err)
			}
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			op, _, _ := operations.GetOperation(ctx, candidate.ID)
			if op.Stage != "committed" || len(op.Tickets) != len(actors) {
				t.Fatal("必须全员提交后发布")
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
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			state, _ = queue.ReadMatchmakingState(ctx, actors[0].PlayerID)
			if state.State != matchmaking.PhaseInMatch {
				t.Fatal("全部真实准入后才进入局内", state.State)
			}
			result := match.Result{EndReason: "Elimination", EndRevision: 1, MatchID: op.MatchID, ArenaModeID: mode, GameServerID: "arena", StartedAt: time.Now().Add(-time.Minute), EndedAt: time.Now(), WinningTeamID: "team-a", Teams: []match.TeamResult{{TeamID: "team-a", Won: true}, {TeamID: "team-b"}}}
			for _, slot := range op.Assignment.Roster {
				result.Players = append(result.Players, match.PlayerResult{PlayerID: slot.PlayerID, TeamID: slot.TeamID, CharacterID: slot.CharacterID})
			}
			if bFault {
				if err = control.CancelArena(ctx, op.MatchID); err != nil {
					t.Fatal(err)
				}
			} else if _, err = control.SubmitMatchResult(ctx, result); err != nil {
				t.Fatal(err)
			}
			if err = runtime.ProcessCandidate(ctx, candidate); err != nil {
				t.Fatal(err)
			}
			for _, actor := range actors {
				migration, err := runtime.GetMigration(ctx, actor, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "return", ClientRequestID: "return-1"})
				if err != nil && !fault.lostReturn {
					t.Fatal(err)
				}
				if err != nil {
					migration, err = runtime.GetMigration(ctx, actor, matchmaking.MigrationRequest{CandidateID: candidate.ID, Direction: "return", ClientRequestID: "return-1"})
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
			}
			if err = control.Heartbeat("arena", 0, gameserver.StatusActive); err != nil {
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
