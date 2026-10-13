package httpadapter

// 端到端HTTP路由契约守卫：普通PlayerData handler不导出可信Grant/Consume端点。
// 真正授予需要服务器身份、玩家会话和事实证据；不能被调用方猜测内部URL绕过。

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/playerdata"
)

func TestPublicPlayerDataInventoryHandlerDoesNotExposeTrustedMutations(t *testing.T) {
	svc := playerdata.NewService(playerdata.NewMemoryRepository())
	svc.AttachInventory(inventory.NewService(inventory.NewMemoryRepository()))
	handler := NewPlayerDataHandler(svc)

	for _, path := range []string{
		"/internal/v1/playerdata/inventory/grant",
		"/internal/v1/playerdata/inventory/consume",
		"/internal/v1/playerdata/inventory/pickup",
		"/internal/v1/playerdata/inventory/claim-reward",
	} {
		req := httptest.NewRequest(http.MethodPost, path,
			strings.NewReader(`{"playerId":"other-account","operationId":"forged","quantity":999}`))
		rec := httptest.NewRecorder()
		handler.ServeHTTP(rec, req)
		if rec.Code != http.StatusNotFound && rec.Code != http.StatusMethodNotAllowed {
			t.Fatalf("可信奖励/消费不能出现在普通PlayerData handler: %s -> %d", path, rec.Code)
		}
	}
}
