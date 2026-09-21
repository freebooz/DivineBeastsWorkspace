package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/identity"
	"divinebeasts/backend/internal/modules/playerdata"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

// 不带Probe能力的内存开发仓储不能冒充真实业务就绪。
func TestOnlineHTTPProbeRejectsLegacyMemoryRepositories(t *testing.T) {
	i := identity.NewService(identity.NewMemorySessionRepository(), identity.SystemClock{}, identity.CryptoTokenGenerator{}, time.Minute, time.Hour)
	for _, tc := range []struct {
		handler http.Handler
		path    string
	}{{NewIdentityHandler(i), "/internal/v1/identity/probe"}, {NewPlayerDataHandler(playerdata.NewService(playerdata.NewMemoryRepository())), "/internal/v1/playerdata/probe"}} {
		s := httptest.NewServer(tc.handler)
		r, err := http.Get(s.URL + tc.path)
		if err != nil {
			t.Fatal(err)
		}
		r.Body.Close()
		if r.StatusCode != 503 {
			t.Errorf("%s probe=%d", tc.path, r.StatusCode)
		}
		s.Close()
	}
}

// 未知内部错误不可泄露正文，HTTP状态转换必须保持401/403/503差别。
func TestOnlineHTTPClientPreservesSafeErrors(t *testing.T) {
	for _, tc := range []struct {
		status int
		code   string
	}{{401, "AUTH_SESSION_INVALID"}, {403, "AUTH_FORBIDDEN"}, {503, "SERVICE_UNAVAILABLE"}} {
		s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			w.WriteHeader(tc.status)
			_, _ = w.Write([]byte(`{"errorCode":"` + tc.code + `","message":"private-secret"}`))
		}))
		_, err := NewIdentityClient(ClientConfig{BaseURL: s.URL}).Authenticate(context.Background(), "token")
		s.Close()
		var code gateway.ServiceError
		if !errors.As(err, &code) || string(code) != tc.code {
			t.Errorf("status=%d got=%v", tc.status, err)
		}
	}
}
func TestOnlineHTTPClientDoesNotRedirectSecrets(t *testing.T) {
	leaked := false
	destination := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { leaked = true; w.WriteHeader(200) }))
	defer destination.Close()
	origin := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { http.Redirect(w, r, destination.URL, 307) }))
	defer origin.Close()
	_, _ = NewIdentityClient(ClientConfig{BaseURL: origin.URL}).Login(context.Background(), gateway.LoginRequest{Credential: "test-only-secret"})
	if leaked {
		t.Fatal("认证正文跨重定向泄漏")
	}
}

// 下游错误的成功状态/空身份/错玩家正文也必须失败，不能转化为网关成功。
func TestOnlineHTTPClientRejectsMalformedSuccess(t *testing.T) {
	for _, body := range []string{`{}`, `{"playerId":"wrong-player","found":true,"gameId":"g","dataVersion":1,"revision":0}`} {
		s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			_, _ = w.Write([]byte(body))
		}))
		if _, err := NewIdentityClient(ClientConfig{BaseURL: s.URL}).Login(context.Background(), gateway.LoginRequest{Provider: "password"}); err == nil {
			t.Error("残缺会话不能成功")
		}
		if _, err := NewPlayerDataClient(ClientConfig{BaseURL: s.URL}).GetProfile(context.Background(), "player-a"); err == nil {
			t.Error("错误玩家资料不能成功")
		}
		if err := NewIdentityClient(ClientConfig{BaseURL: s.URL}).Logout(context.Background(), "token"); err == nil {
			t.Error("撤销必须收到204")
		}
		s.Close()
	}
}
