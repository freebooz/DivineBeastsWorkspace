package gateway

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

// 下游替身仅用于网关边界测试；不代表数据库或真实认证已通过。
type onlineIdentityFake struct {
	fakeIdentity
	probeErr, authErr error
	probes            int
}

func (f *onlineIdentityFake) Probe(context.Context) error { f.probes++; return f.probeErr }
func (f *onlineIdentityFake) Authenticate(ctx context.Context, token string) (AuthenticatedSession, error) {
	if f.authErr != nil {
		return AuthenticatedSession{}, f.authErr
	}
	return f.fakeIdentity.Authenticate(ctx, token)
}
func (f *onlineIdentityFake) Refresh(ctx context.Context, token string) (LoginResponse, error) {
	if token != "refresh-1" {
		return LoginResponse{}, ErrUnauthorized
	}
	return f.Login(ctx, LoginRequest{})
}
func (f *onlineIdentityFake) Logout(_ context.Context, token string) error {
	if token != "refresh-1" {
		return ErrUnauthorized
	}
	return nil
}

type onlinePlayerFake struct {
	fakePlayerData
	probeErr                 error
	probes                   int
	updatedPlayer, name, key string
	revision                 int64
	ensured                  bool
}

func (f *onlinePlayerFake) Probe(context.Context) error { f.probes++; return f.probeErr }
func (f *onlinePlayerFake) EnsureProfile(_ context.Context, playerID, gameID string) error {
	f.ensured = playerID == "player-1" && gameID == "divine-beasts"
	return nil
}
func (f *onlinePlayerFake) UpdateDisplayNameIdempotent(_ context.Context, id, name string, rev int64, key string) (PlayerProfile, error) {
	f.updatedPlayer = id
	f.name = name
	f.revision = rev
	f.key = key
	return PlayerProfile{PlayerID: id, GameID: "divine-beasts", DisplayName: name, Revision: rev + 1, DataVersion: 1}, nil
}
func onlineRequest(h http.Handler, method, path, body string) *httptest.ResponseRecorder {
	r := httptest.NewRequest(method, path, strings.NewReader(body))
	r.Header.Set("Content-Type", "application/json")
	r.Header.Set("Authorization", "Bearer access-1")
	r.Header.Set("Idempotency-Key", "update-1")
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	return w
}

// 证明业务探测真的调用两个端口，且任一存储不可用不能被宿主健康状态遮盖。
func TestOnlineProbeChecksBothDependencies(t *testing.T) {
	for _, failed := range []bool{false, true} {
		i := &onlineIdentityFake{}
		p := &onlinePlayerFake{}
		if failed {
			p.probeErr = errors.New("private database address")
		}
		h := NewAPI(Config{ContractVersion: "1.0.0"}, i, p, fakeParty{}, fakeMatchmaking{})
		w := onlineRequest(h, "GET", "/v1/online/probe", "")
		want := 200
		if failed {
			want = 503
		}
		if w.Code != want || i.probes != 1 || p.probes != 1 || strings.Contains(w.Body.String(), "private database") {
			t.Fatalf("probe status=%d body=%s calls=%d/%d", w.Code, w.Body, i.probes, p.probes)
		}
	}
}

// 断开身份服务必须503，而不是诱导客户端刷新或重新登录的401。
func TestOnlineAuthenticationOutageIsNotUnauthorized(t *testing.T) {
	i := &onlineIdentityFake{authErr: errors.New("private connection detail")}
	h := NewAPI(Config{}, i, &onlinePlayerFake{}, fakeParty{}, fakeMatchmaking{})
	w := onlineRequest(h, "GET", "/v1/player/profile", "")
	if w.Code != 503 || strings.Contains(w.Body.String(), "private") {
		t.Fatalf("status=%d body=%s", w.Code, w.Body)
	}
}

// 白名单、必填与单JSON对象检查必须在调用真实身份服务前生效。
func TestOnlineRejectsMalformedLogin(t *testing.T) {
	for _, body := range []string{`{}`, `null`, `{"gameId":"g","provider":"password","credential":"pw","clientVersion":"1"}`, `{"gameId":"g","provider":"guest","credential":"x","clientVersion":"1","deviceId":"d"} {}`, `{"gameId":"g","gameId":"other","provider":"guest","credential":"x","clientVersion":"1"}`, `{"GameId":"g","provider":"guest","credential":"x","clientVersion":"1"}`} {
		w := onlineRequest(newTestAPI(), "POST", "/v1/auth/login", body)
		if w.Code != 400 {
			t.Errorf("status=%d malformed=%s", w.Code, body)
		}
	}
	w := onlineRequest(newTestAPI(), "POST", "/v1/auth/login", `{"credential":"`+strings.Repeat("x", 70000)+`"}`)
	if w.Code != 413 {
		t.Errorf("oversized status=%d", w.Code)
	}
}

func TestOnlineProfileUsesAuthenticatedSubjectAndStrictPatch(t *testing.T) {
	p := &onlinePlayerFake{}
	h := NewAPI(Config{}, &onlineIdentityFake{}, p, fakeParty{}, fakeMatchmaking{})
	w := onlineRequest(h, "PATCH", "/v1/player/profile", `{"displayName":"  新名字  ","expectedRevision":3}`)
	if w.Code != 200 || p.updatedPlayer != "player-1" || p.name != "新名字" || p.revision != 3 || p.key != "update-1" {
		t.Fatalf("patch status=%d body=%s", w.Code, w.Body)
	}
	for _, body := range []string{`{"displayName":"x"}`, `{"displayName":"x","expectedRevision":null}`, `{"displayName":"x","expectedRevision":-1}`, `{"displayName":"x","expectedRevision":0,"playerId":"victim"}`} {
		if w := onlineRequest(h, "PATCH", "/v1/player/profile", body); w.Code != 400 {
			t.Errorf("invalid patch status=%d", w.Code)
		}
	}
	if w := onlineRequest(h, "GET", "/v1/player/profile?playerId=victim", ""); w.Code != 400 {
		t.Errorf("client playerId status=%d", w.Code)
	}
}

func TestOnlineRefreshLogoutAndLoginInitialization(t *testing.T) {
	p := &onlinePlayerFake{}
	h := NewAPI(Config{}, &onlineIdentityFake{}, p, fakeParty{}, fakeMatchmaking{})
	if w := onlineRequest(h, "POST", "/v1/auth/refresh", `{"refreshToken":"refresh-1"}`); w.Code != 200 {
		t.Errorf("refresh=%d", w.Code)
	}
	if w := onlineRequest(h, "POST", "/v1/auth/logout", `{"refreshToken":"refresh-1"}`); w.Code != 204 || w.Body.Len() != 0 {
		t.Errorf("logout=%d body=%s", w.Code, w.Body)
	}
	w := onlineRequest(h, "POST", "/v1/auth/login", `{"gameId":"divine-beasts","provider":"password","accountName":"a","credential":"pw","clientVersion":"1"}`)
	if w.Code != 200 || !p.ensured {
		t.Fatalf("login=%d ensured=%v", w.Code, p.ensured)
	}
}

func TestOnlineLoginRateIsBounded(t *testing.T) {
	h := newTestAPI()
	limited := false
	for n := 0; n < 100; n++ {
		w := onlineRequest(h, "POST", "/v1/auth/login", `{}`)
		if w.Code == 429 {
			limited = true
			break
		}
	}
	if !limited {
		t.Fatal("登录没有有界限流")
	}
}
