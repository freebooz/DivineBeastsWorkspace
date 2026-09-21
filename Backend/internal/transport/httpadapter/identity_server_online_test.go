package httpadapter

import (
 "context"
 "errors"
 "net/http"
 "net/http/httptest"
 "testing"
 "time"
 "divinebeasts/backend/internal/app/gateway"
 "divinebeasts/backend/internal/modules/identity"
 "divinebeasts/backend/internal/modules/playerdata"
)

// 不带Probe能力的内存开发仓储不能冒充真实业务就绪。
func TestOnlineHTTPProbeRejectsLegacyMemoryRepositories(t *testing.T) {
 i:=identity.NewService(identity.NewMemorySessionRepository(),identity.SystemClock{},identity.CryptoTokenGenerator{},time.Minute,time.Hour)
 for _,handler:=range []http.Handler{NewIdentityHandler(i),NewPlayerDataHandler(playerdata.NewService(playerdata.NewMemoryRepository()))} {
 s:=httptest.NewServer(handler)
 for _,path:=range []string{"/internal/v1/identity/probe","/internal/v1/playerdata/probe"} {r,err:=http.Get(s.URL+path);if err!=nil {t.Fatal(err)};r.Body.Close();if r.StatusCode==404 {continue};if r.StatusCode!=503 {t.Errorf("probe=%d",r.StatusCode)}}
 s.Close()
 }
}
// 未知内部错误不可泄露正文，HTTP状态转换必须保持401/403/503差别。
func TestOnlineHTTPClientPreservesSafeErrors(t *testing.T) {
 for _,tc:=range []struct{status int;code string}{{401,"AUTH_SESSION_INVALID"},{403,"AUTH_FORBIDDEN"},{503,"SERVICE_UNAVAILABLE"}} {
 s:=httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter,r *http.Request){w.Header().Set("Content-Type","application/json");w.WriteHeader(tc.status);_,_=w.Write([]byte(`{"errorCode":"`+tc.code+`","message":"private-secret"}`))}))
 _,err:=NewIdentityClient(ClientConfig{BaseURL:s.URL}).Authenticate(context.Background(),"token");s.Close()
 var code gateway.ServiceError;if !errors.As(err,&code) || string(code)!=tc.code {t.Errorf("status=%d got=%v",tc.status,err)}
 }
}
func TestOnlineHTTPClientDoesNotRedirectSecrets(t *testing.T) {
 leaked:=false;destination:=httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter,r *http.Request){leaked=true;w.WriteHeader(200)}));defer destination.Close()
 origin:=httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter,r *http.Request){http.Redirect(w,r,destination.URL,307)}));defer origin.Close()
 _,_ = NewIdentityClient(ClientConfig{BaseURL:origin.URL}).Login(context.Background(),gateway.LoginRequest{Credential:"test-only-secret"})
 if leaked {t.Fatal("认证正文跨重定向泄漏")}
}
