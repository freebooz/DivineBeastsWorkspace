// 本文件验证网络结果未知后的同身份签票重试；同票据不能返回不同Nonce/Epoch，也不能用同ID覆盖不同目标。
package servertransfer

import (
	"reflect"
	"testing"
	"time"
)

// TestRepeatedIssueUsesStableCredential使进程恢复可安全重放已持久的签票意图，而非给部分玩家生成两套准入。
func TestRepeatedIssueUsesStableCredential(t *testing.T) {
	now := time.Now().UTC()
	service := NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	request := IssueRequest{TicketID: "stable-transfer", AssignmentID: "match:one", GameID: "divine-beasts", PlayerID: "one", SessionID: "session-one", DestinationGameServerID: "arena-one", DestinationEndpoint: "127.0.0.1:17782", DestinationWorldID: "World.MainArena", DestinationExperienceID: "Experience.MainArena.Main", MatchID: "one", TTL: time.Minute, DestinationServerBootID: "boot-one", DestinationProtocolVersion: 1}
	first, err := service.Issue(request)
	if err != nil {
		t.Fatal(err)
	}
	second, err := service.Issue(request)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(first, second) {
		t.Fatal("相同签票意图重试生成了不同秘密/Epoch，无法安全恢复")
	}
	request.DestinationGameServerID = "other"
	if _, err := service.Issue(request); err == nil {
		t.Fatal("同一票据身份不能覆盖目标")
	}
}
