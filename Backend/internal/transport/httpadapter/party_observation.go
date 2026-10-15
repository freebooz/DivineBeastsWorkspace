// 本文件将组队观察连接到现有Match服务；仅此阻塞读使用25秒超时，普通写命令仍保留5秒预算。
package httpadapter

import (
 "context"
 "net/http"
 "time"
 "divinebeasts/backend/internal/app/gateway"
 "divinebeasts/backend/internal/app/matchapi"
 "divinebeasts/backend/internal/modules/party"
)

// registerPartyObservation只注册可信内部路由；主体由认证后的Gateway传入。
func registerPartyObservation(mux *http.ServeMux,service *matchapi.Service){
 mux.HandleFunc("POST /internal/v1/match/party/observe",func(w http.ResponseWriter,r *http.Request){
  var request struct{PlayerID string `json:"playerId"`;Cursor string `json:"cursor"`;WaitMilliseconds int64 `json:"waitMilliseconds"`}
  if err:=gateway.DecodeOnlineJSON(r,&request,[]string{"playerId","cursor","waitMilliseconds"},[]string{"playerId","cursor","waitMilliseconds"});err!=nil{writePartyInteractionError(w,err);return}
  if request.WaitMilliseconds<0||request.WaitMilliseconds>20000{writePartyInteractionError(w,gateway.ServiceError("INVALID_REQUEST"));return}
  result,err:=service.ObserveParty(r.Context(),request.PlayerID,request.Cursor,time.Duration(request.WaitMilliseconds)*time.Millisecond);if err!=nil{writePartyInteractionError(w,err);return};writeJSON(w,http.StatusOK,result)
 })
}

// ObserveParty保留原共享Transport与重定向保护，但单独延长只读等待期限，Context取消仍可提前结束。
func(c *MatchClient)ObserveParty(ctx context.Context,actor,cursor string,wait time.Duration)(party.Observation,error){
 client:=*c.internalClient.client;client.Timeout=25*time.Second
 observation:=internalClient{baseURL:c.baseURL,client:&client}
 var result party.Observation
 err:=observation.post(ctx,"/internal/v1/match/party/observe",map[string]any{"playerId":actor,"cursor":cursor,"waitMilliseconds":wait.Milliseconds()},&result)
 return result,err
}

var _ gateway.PartyObservationPort=(*MatchClient)(nil)
