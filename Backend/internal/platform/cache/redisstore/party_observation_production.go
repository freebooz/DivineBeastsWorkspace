//go:build productiondeps

// 本文件提供单逻辑Redis写域的本人事件观察；Stream随聚合事务写入，PubSub只唤醒等待，断线以持久游标恢复。
package redisstore

import (
 "context"
 "errors"
 "time"
 "divinebeasts/backend/internal/app/matchapi"
)

const partyChangesPrefix="party-changes:" // 本人独立事件流，保留约256条且与聚合具有同级生命周期。

// CurrentPartyCursor读取最近事件；空流返回0-0。Redis不可用必须失败，不能转换成无队伍。
func(r *PartyRepository)CurrentPartyCursor(ctx context.Context,actor string)(string,error){
 entries,err:=r.client.inner.XRevRangeN(ctx,partyChangesPrefix+actor,"+","-",1).Result()
 if err!=nil{return "",err};if len(entries)==0{return "0-0",nil};return entries[0].ID,nil
}

// WaitPartyChange从已读游标续接，最多20秒；截断或流重建显式Resync，取消时关闭本观察专有PubSub连接。
func(r *PartyRepository)WaitPartyChange(ctx context.Context,actor,cursor string,wait time.Duration)(string,bool,error){
 if !matchapi.ValidObservationCursor(cursor)||cursor==""||wait<0||wait>20*time.Second{return "",false,errors.New("PARTY_COMMAND_INVALID: 观察游标或等待时间无效")}
 key:=partyChangesPrefix+actor
 first,err:=r.client.inner.XRangeN(ctx,key,"-","+",1).Result();if err!=nil{return "",false,err}
 current,err:=r.CurrentPartyCursor(ctx,actor);if err!=nil{return "",false,err}
 // 事件ID是数值对；借助Redis区间判断保留窗口，不能用字符串字典序比较毫秒位数。
 known:=cursor=="0-0"||cursor==current
 if !known{entries,err:=r.client.inner.XRangeN(ctx,key,cursor,cursor,1).Result();if err!=nil{return "",false,err};known=len(entries)==1}
 resync:=!known||(len(first)>0&&cursor=="0-0"&&current!="0-0")
 if current!=cursor||resync||wait==0{return current,resync,nil}
 // 先完成订阅确认再复读持久游标，覆盖第一次读与订阅之间的提交；PubSub丢包不影响重连后的日志恢复。
 subscription:=r.client.inner.Subscribe(ctx,key);defer subscription.Close()
 handshake,cancel:=context.WithTimeout(ctx,5*time.Second);defer cancel()
 if _,err:=subscription.Receive(handshake);err!=nil{return "",false,err}
 after,err:=r.CurrentPartyCursor(ctx,actor);if err!=nil{return "",false,err};if after!=cursor{return after,false,nil}
 timer:=time.NewTimer(wait);defer timer.Stop()
 select{case <-ctx.Done():return "",false,ctx.Err();case _,open:=<-subscription.Channel():if !open{return "",false,errors.New("PARTY_OBSERVATION_UNAVAILABLE: 事件连接已断开")};case <-timer.C:return current,false,nil}
 next,err:=r.CurrentPartyCursor(ctx,actor);return next,false,err
}

var _ matchapi.PartyChangeRepository=(*PartyRepository)(nil)
