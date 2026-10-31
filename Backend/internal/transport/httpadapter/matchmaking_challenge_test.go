package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
	"fmt"
	"net/http/httptest"
	"sync/atomic"
	"testing"
	"time"
)

// 真实HTTP客户端/handler往返必须保留新增命令和邀请字段，不另建DTO或绕过严格内部解码。
func TestChallengeHTTPTransport(t *testing.T) {
	var id atomic.Int64
	s := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, id.Add(1)) })
	if err := s.ConfigureMatchmaking(matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only", ContentRevision: "test-only", ProjectRuleRevision: 1, AllowedRegions: []string{"region"}}); err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(NewMatchHandler(s, "test-only-bearer"))
	defer server.Close()
	client := NewMatchClient(ClientConfig{BaseURL: server.URL, BearerToken: "test-only-bearer"})
	actor := func(player string) matchmaking.ActorContext {
		return matchmaking.ActorContext{PlayerID: player, SessionID: "session-" + player, CharacterID: "character-" + player, HeroDefinitionID: "Hero.Zodiac.Rat", CharacterRevision: 1}
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()
	invite, err := client.CommandMatchmaking(ctx, actor("owner"), matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: "target", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
	if err != nil {
		t.Fatal(err)
	}
	if invite.Challenge == nil || !invite.Challenge.CanCancel || invite.Challenge.CanReply {
		t.Fatalf("HTTP邀请字段丢失: %+v", invite)
	}
	target, err := client.ReadMatchmakingState(ctx, "target")
	if err != nil || target.Challenge == nil || !target.Challenge.CanReply {
		t.Fatal("HTTP目标投影错误", err)
	}
	accepted, err := client.CommandMatchmaking(ctx, actor("target"), matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: target.Challenge.ChallengeID, ExpectedRevision: target.Challenge.Revision, Accept: true})
	if err != nil {
		t.Fatal(err)
	}
	if accepted.State != matchmaking.PhaseConfirmation || accepted.Challenge != nil || len(accepted.Participants) != 2 {
		t.Fatal("HTTP固定候选未转换")
	}
}
