//go:build grpcdeps

package grpcadapter

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	mv "divinebeasts/backend/internal/generated/match/v1"
	"divinebeasts/backend/internal/modules/matchmaking"
	"encoding/json"
	"fmt"
	"google.golang.org/grpc/metadata"
	"sync/atomic"
	"testing"
)

// 真实RPC适配方法执行同一严格解码和Service事务；现有JSON Envelope不需复制一套Proto业务字段。
func TestChallengeGRPCAdapter(t *testing.T) {
	var id atomic.Int64
	s := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, id.Add(1)) })
	if err := s.ConfigureMatchmaking(matchapi.MatchmakingSettings{Enabled: true, Development: true, MapID: "test-only", ContentRevision: "test-only", ProjectRuleRevision: 1, AllowedRegions: []string{"region"}}); err != nil {
		t.Fatal(err)
	}
	rpc := &MatchAPIServer{service: s, runtimeBearer: "test-only-bearer"}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "Bearer test-only-bearer"))
	call := func(player string, command matchmaking.Command) matchmaking.PlayerSnapshot {
		t.Helper()
		a, _ := json.Marshal(matchmaking.ActorContext{PlayerID: player, SessionID: "s-" + player, CharacterID: "c-" + player, HeroDefinitionID: "Hero.Zodiac.Rat", CharacterRevision: 1})
		c, _ := json.Marshal(command)
		response, err := rpc.CommandMatchmaking(ctx, &mv.MatchmakingCommandRequest{ActorJson: string(a), CommandJson: string(c)})
		if err != nil || response.GetErrorCode() != "" {
			t.Fatal("约战RPC失败", err, response.GetErrorCode())
		}
		var state matchmaking.PlayerSnapshot
		if err := json.Unmarshal([]byte(response.GetPayloadJson()), &state); err != nil {
			t.Fatal(err)
		}
		return state
	}
	invite := call("owner", matchmaking.Command{Action: "challenge", ClientRequestID: "invite", TargetPlayerID: "target", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "region"})
	if invite.Challenge == nil {
		t.Fatal("RPC邀请字段未序列化")
	}
	accepted := call("target", matchmaking.Command{Action: "challengeReply", ClientRequestID: "accept", ChallengeID: invite.Challenge.ChallengeID, ExpectedRevision: invite.Challenge.Revision, Accept: true})
	if accepted.State != matchmaking.PhaseConfirmation || accepted.Challenge != nil || len(accepted.Participants) != 2 {
		t.Fatal("RPC没有进入同一固定候选")
	}
}
