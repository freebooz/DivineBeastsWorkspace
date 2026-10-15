// 本文件暴露已认证本人组队观察；Gateway只解析合同并传递Context，不持有队伍权威或伪造空快照。
package gateway

import (
 "context"
 "net/http"
 "time"
 "divinebeasts/backend/internal/modules/party"
)

// PartyObservationPort由真实HTTP/gRPC适配实现；独立于旧查询端口，缺失时明确失败关闭。
type PartyObservationPort interface {ObserveParty(context.Context,string,string,time.Duration)(party.Observation,error)}

// observeParty禁止客户端提交actor；每个请求只拥有一次有界观察，其取消传递到仓储。
func(a *api)observeParty(w http.ResponseWriter,r *http.Request,session AuthenticatedSession){
 port,ok:=a.party.(PartyObservationPort);if !ok{writeOnlineError(w,ServiceError("SERVICE_UNAVAILABLE"));return}
 var request struct{Cursor string `json:"cursor"`;WaitMilliseconds int64 `json:"waitMilliseconds"`}
 if r.URL.RawQuery!=""{writeOnlineError(w,ServiceError("INVALID_REQUEST"));return}
 if err:=DecodeOnlineJSON(r,&request,[]string{"cursor","waitMilliseconds"},[]string{"cursor","waitMilliseconds"});err!=nil{writeOnlineError(w,err);return}
 if request.WaitMilliseconds<0||request.WaitMilliseconds>20000||len(request.Cursor)>43{writeOnlineError(w,ServiceError("INVALID_REQUEST"));return}
 result,err:=port.ObserveParty(r.Context(),session.PlayerID,request.Cursor,time.Duration(request.WaitMilliseconds)*time.Millisecond)
 if err!=nil{writePartyError(w,err);return};w.Header().Set("Cache-Control","no-store");writeJSON(w,http.StatusOK,result)
}
