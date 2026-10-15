// 本回归启动真实HTTP Handler/Client，核对分配元数据、稳定签票、Boot限定观察与取消；领域内存仓储仅为测试配置。
package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	contracts "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

// TestArenaRuntimeHTTP真实往返HTTP字段和TTL；不使用固定成功的服务替身，也不输出准入秘密。
func TestArenaRuntimeHTTP(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	service := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now), match.NewResultService(match.NewMemoryResultStore()), time.Now)
	if err := service.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
		t.Fatal(err)
	}
	registration := gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "http-arena", ServerBootID: "http-boot", ServerRoleID: contracts.RoleMainArena, ExperienceID: contracts.ExperienceMainArenaMain, WorldID: "divinebeasts.world.main_arena@1", RegionID: "test", PublicEndpoint: "127.0.0.1:17782", BuildVersion: "test-only", ProtocolVersion: 1, Capacity: 10}
	if err := service.Register(registration); err != nil {
		t.Fatal(err)
	}
	if err := service.SetReady(registration.GameServerID); err != nil {
		t.Fatal(err)
	}
	host := httptest.NewServer(NewGameServerControlHandler(service, "test-only-internal"))
	defer host.Close()
	client := NewGameServerControlClient(GameServerControlClientConfig{ClientConfig: ClientConfig{BaseURL: host.URL}, BearerToken: "test-only-internal", DefaultRegion: "test"})
	input := gameservercontrol.AllocateMainArenaInput{WorldID: registration.WorldID, MatchID: "http-match", ArenaModeID: contracts.ArenaMode1v1, MapID: registration.WorldID, RegionID: "test", ProjectRuleRevision: 1, ContentRevision: "test-only-content", HeroCatalogRevision: 1, Roster: []contracts.RosterPlayer{{PlayerID: "http-a", TeamID: "team-a", CharacterID: "char-a", HeroDefinitionID: "Hero.Zodiac.Rat", SlotIndex: 0}, {PlayerID: "http-b", TeamID: "team-b", CharacterID: "char-b", HeroDefinitionID: "Hero.Zodiac.Ox", SlotIndex: 1}}}
	assignment, err := client.AllocateMainArenaContext(ctx, input)
	if err != nil || assignment.ProjectRuleRevision != 1 || assignment.ContentRevision != input.ContentRevision || assignment.Roster[1].SlotIndex != 1 {
		t.Fatal("分配往返丢失元数据", err)
	}
	issue := gameservercontrol.IssueTransferInput{TicketID: "http-ticket", GameID: "divine-beasts", PlayerID: "http-a", SessionID: "http-session", DestinationGameServerID: registration.GameServerID, MatchID: input.MatchID, TTL: time.Minute}
	ticket, err := client.IssueTransferContext(ctx, issue)
	if err != nil || time.Until(ticket.ExpiresAt) < 50*time.Second {
		t.Fatal("真实签票路径或时长错误", err)
	}
	again, err := client.IssueTransferContext(ctx, issue)
	if err != nil || again != ticket {
		t.Fatal("重试必须返回同一张真实票", err)
	}
	request, _ := http.NewRequestWithContext(ctx, http.MethodGet, host.URL+"/internal/v1/gameservers/assignment?gameServerId=http-arena&waitMilliseconds=100", nil)
	request.Header.Set("Authorization", "Bearer test-only-internal")
	request.Header.Set("X-Game-Server-Id", registration.GameServerID)
	request.Header.Set("X-Game-Server-Boot-Id", "wrong-boot")
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	response.Body.Close()
	if response.StatusCode != http.StatusConflict {
		t.Fatal("旧Boot不能读取分配", response.StatusCode)
	}
	if err := client.RevokeTransfer(ctx, ticket.TicketID); err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateTransferContext(ctx, ticket, registration.GameServerID); err == nil {
		t.Fatal("已撤销票据不能准入")
	}
}
