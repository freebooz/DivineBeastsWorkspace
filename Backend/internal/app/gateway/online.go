package gateway

import (
 "bytes"
 "context"
 "encoding/json"
 "errors"
 "io"
 "mime"
 "net"
 "net/http"
 "strings"
 "sync"
 "time"
 "unicode/utf8"
)

// ProbePort 只由下游业务仓储的真实探测实现；宿主health不满足此能力。
type ProbePort interface { Probe(context.Context) error }
// AuthenticationLifecycle 扩展旧身份端口，不强迫既有开发替身实现新方法。
type AuthenticationLifecycle interface {
 Refresh(context.Context,string) (LoginResponse,error)
 Logout(context.Context,string) error
}
// ProfileInitializer 只调用资料领域幂等建档，不允许网关直接写表。
type ProfileInitializer interface { EnsureProfile(context.Context,string,string) error }
// ProfileUpdater 以已认证主体调用幂等资料更新；修订号和键必须原样下传。
type ProfileUpdater interface { UpdateDisplayNameIdempotent(context.Context,string,string,int64,string) (PlayerProfile,error) }

// ServiceError 是安全的跨适配层错误码；不携带地址、SQL、凭据或原始下游正文。
type ServiceError string
func (e ServiceError) Error() string { return string(e) }
// OnlineErrorStatus 对稳定错误码白名单映射；未知错误一律503，不猜测为凭据错误。
func OnlineErrorStatus(err error) (int,string) {
 if errors.Is(err,ErrUnauthorized) {return 401,"AUTH_SESSION_INVALID"}
 var code ServiceError
 if !errors.As(err,&code) { return 503,"SERVICE_UNAVAILABLE" }
 switch code {
 case "AUTH_SESSION_INVALID","AUTH_INVALID_CREDENTIALS","AUTH_TOKEN_EXPIRED","AUTH_REFRESH_REPLAY": return 401,string(code)
 case "AUTH_FORBIDDEN","AUTH_ACCOUNT_DISABLED": return 403,string(code)
 case "INVALID_REQUEST","AUTH_PROVIDER_UNSUPPORTED":return 400,string(code)
 case "REQUEST_TOO_LARGE":return 413,string(code)
 case "RATE_LIMITED":return 429,string(code)
 case "PLAYER_PROFILE_NOT_FOUND":return 404,string(code)
 case "PLAYER_DATA_CONFLICT","IDEMPOTENCY_CONFLICT":return 409,string(code)
 default:return 503,"SERVICE_UNAVAILABLE"
 }
}
func writeOnlineError(w http.ResponseWriter,err error) {
 status,code:=OnlineErrorStatus(err);w.Header().Set("Cache-Control","no-store")
 writeAPIError(w,status,code,"请求未完成，请按错误码处理")
}

// DecodeOnlineJSON 限制64KiB、UTF8和单对象；精确区分字段大小写、拒绝重复键/null必填/尾随JSON。
// allowed和required源于共享契约；只接受平坦操作DTO，不能用于任意嵌套协议。
func DecodeOnlineJSON(r *http.Request,target any,allowed,required []string) error {
 if ct:=r.Header.Get("Content-Type");ct!="" {media,_,err:=mime.ParseMediaType(ct);if err!=nil || media!="application/json" {return ServiceError("INVALID_REQUEST")}}
 data,err:=io.ReadAll(io.LimitReader(r.Body,65537));if err!=nil {return ServiceError("INVALID_REQUEST")};if len(data)>65536 {return ServiceError("REQUEST_TOO_LARGE")}
 if !utf8.Valid(data) {return ServiceError("INVALID_REQUEST")}
 d:=json.NewDecoder(bytes.NewReader(data));token,err:=d.Token();if err!=nil || token!=json.Delim('{') {return ServiceError("INVALID_REQUEST")}
 permitted:=map[string]bool{};for _,key:=range allowed {permitted[key]=true};seen:=map[string]json.RawMessage{}
 for d.More() {token,err=d.Token();if err!=nil {return ServiceError("INVALID_REQUEST")};key,ok:=token.(string);if !ok || !permitted[key] {return ServiceError("INVALID_REQUEST")};if _,exists:=seen[key];exists {return ServiceError("INVALID_REQUEST")};var value json.RawMessage;if d.Decode(&value)!=nil {return ServiceError("INVALID_REQUEST")};seen[key]=value }
 if _,err=d.Token();err!=nil {return ServiceError("INVALID_REQUEST")};if _,err=d.Token();err!=io.EOF {return ServiceError("INVALID_REQUEST")}
 for _,key:=range required {v,ok:=seen[key];if !ok || bytes.Equal(bytes.TrimSpace(v),[]byte("null")) {return ServiceError("INVALID_REQUEST")}}
 if json.Unmarshal(data,target)!=nil {return ServiceError("INVALID_REQUEST")};return nil
}

func (a *api) probeOnline(w http.ResponseWriter,r *http.Request) {
 ctx,cancel:=context.WithTimeout(r.Context(),5*time.Second);defer cancel()
 ready:=true
 for _,port:=range []any{a.identity,a.playerData} {p,ok:=port.(ProbePort);if !ok {ready=false;continue};if p.Probe(ctx)!=nil {ready=false}}
 if !ready {writeOnlineError(w,ServiceError("SERVICE_UNAVAILABLE"));return}
 writeJSON(w,200,map[string]any{"ready":true,"contractVersion":a.config.ContractVersion,"service":"gatewayservice"})
}
func (a *api) refreshOnline(w http.ResponseWriter,r *http.Request) {
 if !a.allowAuthentication(w,r) {return};token,err:=readRefreshToken(r);if err!=nil {writeOnlineError(w,err);return}
 port,ok:=a.identity.(AuthenticationLifecycle);if !ok {writeOnlineError(w,ServiceError("SERVICE_UNAVAILABLE"));return}
 response,err:=port.Refresh(r.Context(),token);if err!=nil {writeOnlineError(w,err);return}
 w.Header().Set("Cache-Control","no-store");writeJSON(w,200,response)
}
func (a *api) logoutOnline(w http.ResponseWriter,r *http.Request) {
 token,err:=readRefreshToken(r);if err!=nil {writeOnlineError(w,err);return}
 port,ok:=a.identity.(AuthenticationLifecycle);if !ok {writeOnlineError(w,ServiceError("SERVICE_UNAVAILABLE"));return}
 if err:=port.Logout(r.Context(),token);err!=nil {writeOnlineError(w,err);return}
 w.Header().Set("Cache-Control","no-store");w.WriteHeader(204)
}
func readRefreshToken(r *http.Request)(string,error) {
 var req struct {RefreshToken string `json:"refreshToken"`}
 if err:=DecodeOnlineJSON(r,&req,[]string{"refreshToken"},[]string{"refreshToken"});err!=nil {return "",err}
 if strings.TrimSpace(req.RefreshToken)=="" {return "",ServiceError("INVALID_REQUEST")};return req.RefreshToken,nil
}
func (a *api) updateProfile(w http.ResponseWriter,r *http.Request,session AuthenticatedSession) {
 if r.URL.RawQuery!="" {writeOnlineError(w,ServiceError("INVALID_REQUEST"));return}
 var req struct {DisplayName string `json:"displayName"`;ExpectedRevision int64 `json:"expectedRevision"`}
 if err:=DecodeOnlineJSON(r,&req,[]string{"displayName","expectedRevision"},[]string{"displayName","expectedRevision"});err!=nil {writeOnlineError(w,err);return}
 req.DisplayName=strings.TrimSpace(req.DisplayName);key:=r.Header.Get("Idempotency-Key")
 if req.DisplayName=="" || utf8.RuneCountInString(req.DisplayName)>24 || req.ExpectedRevision<0 || strings.TrimSpace(key)=="" || utf8.RuneCountInString(key)>128 {writeOnlineError(w,ServiceError("INVALID_REQUEST"));return}
 port,ok:=a.playerData.(ProfileUpdater);if !ok {writeOnlineError(w,ServiceError("SERVICE_UNAVAILABLE"));return}
 profile,err:=port.UpdateDisplayNameIdempotent(r.Context(),session.PlayerID,req.DisplayName,req.ExpectedRevision,key);if err!=nil {writeOnlineError(w,err);return};writeProfile(w,profile)
}
func writeProfile(w http.ResponseWriter,profile PlayerProfile) {if profile.OwnedCharacterIDs==nil {profile.OwnedCharacterIDs=[]string{}};w.Header().Set("Cache-Control","no-store");writeJSON(w,200,profile)}

type authWindow struct {start time.Time;count int}
type authRateLimiter struct {mu sync.Mutex;limit int;windows map[string]authWindow}
func newAuthRateLimiter(limit int)*authRateLimiter {if limit<=0 {limit=30};return &authRateLimiter{limit:limit,windows:map[string]authWindow{}}}
func (a *api) allowAuthentication(w http.ResponseWriter,r *http.Request) bool {
 host,_,err:=net.SplitHostPort(r.RemoteAddr);if err!=nil {host=r.RemoteAddr}
 l:=a.authLimiter;l.mu.Lock();defer l.mu.Unlock();now:=time.Now()
 if len(l.windows)>=4096 {for k,v:=range l.windows {if now.Sub(v.start)>=time.Minute {delete(l.windows,k)}}}
 entry,exists:=l.windows[host];if !exists && len(l.windows)>=4096 {w.Header().Set("Retry-After","60");writeOnlineError(w,ServiceError("RATE_LIMITED"));return false}
 if entry.start.IsZero() || now.Sub(entry.start)>=time.Minute {entry=authWindow{start:now}}
 if entry.count>=l.limit {w.Header().Set("Retry-After","60");writeOnlineError(w,ServiceError("RATE_LIMITED"));return false}
 entry.count++;l.windows[host]=entry;return true
}
