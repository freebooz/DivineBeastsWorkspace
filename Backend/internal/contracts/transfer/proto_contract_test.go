package transfer

import (
	"os"
	"strings"
	"testing"
)

// TestValidateTransferProtoCarriesSignedTicketContext（迁移验证契约完整性测试）确保目标Server能提交完整签名上下文进行无状态验证。
func TestValidateTransferProtoCarriesSignedTicketContext(t *testing.T) {
	raw, err := os.ReadFile("../../../../Shared/Contracts/GamePlatform/Proto/server-transfer.proto")
	if err != nil {
		t.Fatal(err)
	}
	text := string(raw)
	required := []string{"session_id", "game_id", "source_game_server_id", "destination_world_id", "match_id", "issued_at_unix_ms", "expires_at_unix_ms"}
	for _, field := range required {
		if !strings.Contains(text, field) {
			t.Fatalf("server-transfer.proto缺少完整签名字段: %s", field)
		}
	}
}
