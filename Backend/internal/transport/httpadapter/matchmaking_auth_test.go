// 本回归验证Match内部身份边界；Actor正文不是认证，缺少部署凭据必须先拒绝，不能进入业务解析。
package httpadapter

import (
	"divinebeasts/backend/internal/app/matchapi"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestMatchmakingRejectsUntrustedInternalCaller(t *testing.T) {
	service := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(s string) string { return s + "-test" })
	handler := NewMatchHandler(service)
	for _, path := range []string{"commands", "migration", "state", "observe"} {
		req := httptest.NewRequest(http.MethodPost, "/internal/v1/match/matchmaking/"+path, strings.NewReader(`{}`))
		out := httptest.NewRecorder()
		handler.ServeHTTP(out, req)
		if out.Code != http.StatusUnauthorized {
			t.Fatalf("%s 无凭据应401，实际%d", path, out.Code)
		}
	}
}
