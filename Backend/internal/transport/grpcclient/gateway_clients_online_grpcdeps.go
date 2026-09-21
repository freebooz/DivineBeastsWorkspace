//go:build grpcdeps

package grpcclient

import (
 "context"
 "time"
 "divinebeasts/backend/internal/app/gateway"
 identityv1 "divinebeasts/backend/internal/generated/identity/v1"
 playerdatav1 "divinebeasts/backend/internal/generated/playerdata/v1"
)

// 所有Online RPC有5秒上限并尊重调用方更短截止；登录/刷新绝不自动重试。
func(c *IdentityClient)Refresh(ctx context.Context,token string)(gateway.LoginResponse,error){
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.Refresh(ctx,&identityv1.RefreshRequest{RefreshToken:token});if err!=nil{return gateway.LoginResponse{},err};return loginResponse(out)
}
func loginResponse(out *identityv1.LoginResponse)(gateway.LoginResponse,error){
 if out.GetErrorCode()!=""{return gateway.LoginResponse{},gateway.ServiceError(out.GetErrorCode())}
 if out.GetPlayerId()==""||out.GetSessionId()==""||out.GetAccessToken()==""||out.GetRefreshToken()==""||out.GetExpiresAtUnixMs()<=0||out.GetRefreshExpiresAtUnixMs()<=0{return gateway.LoginResponse{},gateway.ServiceError("SERVICE_UNAVAILABLE")}
 return gateway.LoginResponse{PlayerID:out.GetPlayerId(),SessionID:out.GetSessionId(),AccessToken:out.GetAccessToken(),RefreshToken:out.GetRefreshToken(),ExpiresAt:time.UnixMilli(out.GetExpiresAtUnixMs()).UTC().Format(time.RFC3339Nano),RefreshExpiresAt:time.UnixMilli(out.GetRefreshExpiresAtUnixMs()).UTC().Format(time.RFC3339Nano)},nil
}
func(c *IdentityClient)Logout(ctx context.Context,token string)error{
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.Logout(ctx,&identityv1.RefreshRequest{RefreshToken:token});if err!=nil{return err};if out.GetErrorCode()!=""{return gateway.ServiceError(out.GetErrorCode())};return nil
}
func(c *IdentityClient)Probe(ctx context.Context)error{
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.Probe(ctx,&identityv1.ProbeRequest{});if err!=nil{return err};if !out.GetReady()||out.GetErrorCode()!=""{return gateway.ServiceError("SERVICE_UNAVAILABLE")};return nil
}
func(c *PlayerDataClient)Probe(ctx context.Context)error{
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.Probe(ctx,&playerdatav1.ProbeRequest{});if err!=nil{return err};if !out.GetReady()||out.GetErrorCode()!=""{return gateway.ServiceError("SERVICE_UNAVAILABLE")};return nil
}
func(c *PlayerDataClient)EnsureProfile(ctx context.Context,id,game string)error{
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.EnsureProfile(ctx,&playerdatav1.EnsureProfileRequest{PlayerId:id,GameId:game});if err!=nil{return err};if out.GetErrorCode()!=""{return gateway.ServiceError(out.GetErrorCode())};return nil
}
func(c *PlayerDataClient)UpdateDisplayNameIdempotent(ctx context.Context,id,name string,revision int64,key string)(gateway.PlayerProfile,error){
 ctx,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel();out,err:=c.client.UpdateProfile(ctx,&playerdatav1.UpdateProfileRequest{PlayerId:id,DisplayName:name,ExpectedRevision:&revision,IdempotencyKey:key});if err!=nil{return gateway.PlayerProfile{},err};if out.GetErrorCode()!=""{return gateway.PlayerProfile{},gateway.ServiceError(out.GetErrorCode())};if !out.GetFound()||out.GetPlayerId()!=id||out.GetDataVersion()<1||out.GetRevision()<0{return gateway.PlayerProfile{},gateway.ServiceError("SERVICE_UNAVAILABLE")}
 return gateway.PlayerProfile{PlayerID:out.GetPlayerId(),GameID:out.GetGameId(),DisplayName:out.GetDisplayName(),DataVersion:int(out.GetDataVersion()),Revision:out.GetRevision(),TutorialCompleted:out.GetTutorialCompleted(),DefaultWorldID:out.GetDefaultWorldId(),OwnedCharacterIDs:append([]string{},out.GetOwnedCharacterIds()...)},nil
}
