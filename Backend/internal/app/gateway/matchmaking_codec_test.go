package gateway

import (
	"context"
	"encoding/json"
	"testing"
)

func TestMatchmakingActorUsesSelectedCharacterName(t *testing.T) {
	a := &api{playerData: fakePlayerData{}}
	actor, err := a.authenticatedMatchmakingActor(context.Background(), AuthenticatedSession{PlayerID: "player-1", SessionID: "session-1"}, true)
	if err != nil {
		t.Fatal(err)
	}
	payload, err := json.Marshal(actor)
	if err != nil {
		t.Fatal(err)
	}
	forwarded, err := DecodeMatchmakingActorJSON(string(payload))
	if err != nil {
		t.Fatal(err)
	}
	if forwarded.CharacterName != "白君" {
		t.Fatalf("可信匹配上下文应保留已选角色名，实际为 %q", forwarded.CharacterName)
	}
}

func TestMatchmakingActorAcceptsOlderContext(t *testing.T) {
	const payload = `{"playerId":"player-1","sessionId":"session-1","characterId":"character-1","heroDefinitionId":"Hero.Zodiac.Tiger","characterRevision":"1","sourceGameServerId":"village-1","sourceExperienceId":"Experience.Village.Tutorial"}`
	if actor, err := DecodeMatchmakingActorJSON(payload); err != nil || actor.CharacterID != "character-1" || actor.CharacterName != "" {
		t.Fatalf("可信旧上下文仍应可解析: %v", err)
	}
}

// 公网与HTTP/gRPC内部Envelope共用此严格解码器；新增字段不得导致正文冒名、漏accept或精度截断。
func TestChallengeCommandStrictDecode(t *testing.T) {
	c, err := DecodeMatchmakingCommandJSON(`{"action":"challenge","clientRequestId":"invite","expectedRevision":"0","targetPlayerId":"target","arenaModeId":"Arena.Mode.Duel1v1","regionId":"region"}`)
	if err != nil || c.TargetPlayerID != "target" || c.Action != "challenge" {
		t.Fatal("约战增量字段未受理", err)
	}
	c, err = DecodeMatchmakingCommandJSON(`{"action":"challengeReply","clientRequestId":"accept","expectedRevision":"1","challengeId":"invitation","accept":true}`)
	if err != nil || c.ChallengeID != "invitation" || !c.Accept {
		t.Fatal("邀请回复未完整解析", err)
	}
	for _, payload := range []string{
		`{"action":"challengeReply","clientRequestId":"accept","expectedRevision":"1","challengeId":"id"}`,
		`{"action":"challengeReply","clientRequestId":"accept","expectedRevision":"1","challengeId":"id","accept":null}`,
		`{"action":"challenge","clientRequestId":"invite","expectedRevision":1,"targetPlayerId":"target"}`,
		`{"action":"challenge","clientRequestId":"invite","expectedRevision":"0","targetPlayerId":"target","playerId":"other"}`,
		`{"action":"challenge","clientRequestId":"invite","expectedRevision":"0","targetPlayerId":"target","characterName":"自报姓名"}`,
		`{"action":"challenge","clientRequestId":"invite","expectedRevision":"0","targetPlayerId":"target","targetPlayerId":"other"}`,
	} {
		if _, err := DecodeMatchmakingCommandJSON(payload); err == nil {
			t.Fatalf("非法命令被接受: %s", payload)
		}
	}
}
