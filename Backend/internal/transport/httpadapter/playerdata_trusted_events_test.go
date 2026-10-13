package httpadapter

// 可信GameServer事件HTTP门禁测试：普通客户端无法调用，只有独立内部Bearer、
// 正确GameServer与Boot头以及真实会话校验三关通过后，才能提交服务端已确认事件。

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/playerdata"
)

const trustedTestSecret="only-server-internal-inventory-secret-32-bytes"

type trustedHTTPVerifier struct{}
func(trustedHTTPVerifier) VerifyServerPlayer(_ context.Context,lease inventory.TrustedPlayerLease)error{
	if lease.GameID!="divine-beasts" || lease.PlayerID!="player-http" ||
		lease.GameServerID!="village-1" || lease.ServerBootID!="boot-1" ||
		lease.SessionEpoch!=9 || lease.ConnectionID!="connection-9" {
		return errors.New("世界准入不存在")
	}
	return nil
}
type trustedHTTPJournal struct{ count int }
func(j *trustedHTTPJournal) RecordConfirmedEvent(_ context.Context,e inventory.ConfirmedInventoryEvent)error{
	j.count++;return nil
}
type trustedHTTPMutations struct{ count int }
func(m *trustedHTTPMutations) GetSnapshot(_ context.Context,_ string)(inventory.Snapshot,error){
	return inventory.Snapshot{InventoryRevision:1},nil
}
func(m *trustedHTTPMutations) GrantTrusted(_ context.Context,_ string,c inventory.GrantCommand)(inventory.MutationResult,error){
	m.count++;return inventory.MutationResult{OperationID:c.OperationID,
		Snapshot:inventory.Snapshot{InventoryRevision:c.ExpectedRevision+1}},nil
}
func(m *trustedHTTPMutations) ConsumeTrusted(_ context.Context,_ string,c inventory.ConsumeCommand)(inventory.MutationResult,error){
	m.count++;return inventory.MutationResult{OperationID:c.OperationID,
		Snapshot:inventory.Snapshot{InventoryRevision:c.ExpectedRevision+1}},nil
}

func TestTrustedInventoryEventHTTPIsSeparateAndAuthenticated(t *testing.T){
	journal:=&trustedHTTPJournal{}
	mutation:=&trustedHTTPMutations{}
	proc,err:=inventory.NewTrustedEventProcessor("divine-beasts",trustedHTTPVerifier{},journal,mutation)
	if err!=nil{t.Fatal(err)}
	service:=playerdata.NewService(playerdata.NewMemoryRepository())
	handler:=NewPlayerDataHandlerWithTrustedEvents(service,proc,trustedTestSecret)
	event:=inventory.ConfirmedInventoryEvent{
		EventKind:inventory.TrustedEventGrant,GameID:"divine-beasts",
		GameServerID:"village-1",ServerBootID:"boot-1",PlayerID:"player-http",
		SessionEpoch:9,ConnectionID:"connection-9",SourceEventID:"source-1",
		ItemDefinitionID:"item.herb.peach",Quantity:2,ReasonCode:"pickup",
		ExpectedRevision:1,
	}
	payload,_:=json.Marshal(event)
	makeCall:=func(token,serverID,bootID string,body []byte)int{
		t.Helper()
		req:=httptest.NewRequest(http.MethodPost,
			"/internal/v1/playerdata/inventory/trusted-events",bytes.NewReader(body))
		req.Header.Set("Content-Type","application/json")
		if token!="" {req.Header.Set("Authorization","Bearer "+token)}
		if serverID!="" {req.Header.Set("X-Game-Server-Id",serverID)}
		if bootID!="" {req.Header.Set("X-Game-Server-Boot-Id",bootID)}
		rec:=httptest.NewRecorder()
		handler.ServeHTTP(rec,req)
		return rec.Code
	}
	if status:=makeCall("","village-1","boot-1",payload);status!=401{
		t.Fatalf("缺少可信令牌必须拒绝: %d",status)
	}
	if status:=makeCall("guess-token","village-1","boot-1",payload);status!=401{
		t.Fatalf("错误令牌必须拒绝: %d",status)
	}
	if status:=makeCall(trustedTestSecret,"old-village","boot-1",payload);status!=403{
		t.Fatalf("HTTP实例身份不匹配必须拒绝: %d",status)
	}
	if status:=makeCall(trustedTestSecret,"village-1","old-boot",payload);status!=403{
		t.Fatalf("Boot头不匹配必须拒绝: %d",status)
	}
	invalid:=append([]byte(nil),payload...)
	invalid=append(invalid,[]byte(`{"forge":true}`)...)
	if status:=makeCall(trustedTestSecret,"village-1","boot-1",invalid);status!=400{
		t.Fatalf("尾随JSON必须拒绝: %d",status)
	}
	badLease:=event;badLease.SessionEpoch=1
	badBody,_:=json.Marshal(badLease)
	status:=makeCall(trustedTestSecret,"village-1","boot-1",badBody)
	if status==200{t.Fatalf("已过期玩家世代不允许写入: %d",status)}
	if journal.count!=0 || mutation.count!=0{
		t.Fatal("HTTP/会话认证失败不能碰触事件日志和库存")
	}
	if status:=makeCall(trustedTestSecret,"village-1","boot-1",payload);status!=200{
		t.Fatalf("合法已确认事件应转发到受控处理器: %d",status)
	}
	if journal.count!=1 || mutation.count!=1{
		t.Fatal("可信日志与事务调用必须严格一次")
	}
	req:=httptest.NewRequest(http.MethodPost,
		"/internal/v1/playerdata/inventory/trusted-events",bytes.NewReader(payload))
	rec:=httptest.NewRecorder()
	NewPlayerDataHandler(service).ServeHTTP(rec,req)
	if rec.Code!=http.StatusNotFound && rec.Code!=http.StatusMethodNotAllowed{
		t.Fatalf("普通PlayerData路由必须始终关闭可信事件: %d",rec.Code)
	}
}
