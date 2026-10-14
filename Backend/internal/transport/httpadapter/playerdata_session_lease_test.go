package httpadapter

// 服务器玩家租约只能同Boot/同世代/同连接续期，不能由普通客户端创建。
import (
    "bytes"
    "context"
    "errors"
    "net/http"
    "net/http/httptest"
    "testing"
    "time"

    "divinebeasts/backend/internal/modules/inventory"
    "divinebeasts/backend/internal/modules/playerdata"
)

type recordingLeaseRenewer struct {
    calls int
    expiry time.Time
}

func (r *recordingLeaseRenewer) VerifyServerPlayer(ctx context.Context, lease inventory.TrustedPlayerLease) error {
    if lease.GameID!="divine-beasts" || lease.GameServerID!="village-1" ||
        lease.ServerBootID!="boot-1" || lease.PlayerID!="player-http" ||
        lease.SessionEpoch!=9 || lease.ConnectionID!="connection-9" {
        return errors.New("真实会话不存在")
    }
    return nil
}
func (r *recordingLeaseRenewer) RenewServerLease(
    ctx context.Context, lease inventory.TrustedPlayerLease, secs int32,
) (time.Time,error) {
    if err:=r.VerifyServerPlayer(ctx,lease);err!=nil {
        return time.Time{},err
    }
    if secs<1 || secs>inventory.MaxInventorySessionLeaseSeconds {
        return time.Time{},errors.New("单次租约越界")
    }
    r.calls++
    return r.expiry,nil
}

func TestTrustedInventorySessionRenewRejectsForgery(t *testing.T) {
    lease:=&recordingLeaseRenewer{
        expiry:time.Date(2026,10,13,17,42,30,0,time.UTC),
    }
    handler:=NewTrustedInventorySessionRenewHandler(lease,trustedTestSecret)
    valid:=`{"gameId":"divine-beasts","playerId":"player-http","gameServerId":"village-1","serverBootId":"boot-1","sessionEpoch":9,"connectionId":"connection-9","leaseSeconds":20}`
    send:=func(method,token,server,boot,body string)int{
        t.Helper()
        req:=httptest.NewRequest(method,"/internal/v1/playerdata/inventory/session-renew",
            bytes.NewBufferString(body))
        req.Header.Set("Authorization","Bearer "+token)
        req.Header.Set("Content-Type","application/json")
        req.Header.Set("X-Game-Server-Id",server)
        req.Header.Set("X-Game-Server-Boot-Id",boot)
        response:=httptest.NewRecorder()
        handler.ServeHTTP(response,req)
        return response.Code
    }
    if status:=send(http.MethodGet,trustedTestSecret,"village-1","boot-1",valid);status!=405 {t.Fatalf("续租错误Method=%d",status)}
    if status:=send(http.MethodPost,"guessed","village-1","boot-1",valid);status!=401 {t.Fatalf("未验证来源必须401: %d",status)}
    if status:=send(http.MethodPost,trustedTestSecret,"other-server","boot-1",valid);status!=403 {t.Fatalf("实例不一致必须403: %d",status)}
    if status:=send(http.MethodPost,trustedTestSecret,"village-1","old-boot",valid);status!=403 {t.Fatalf("Boot不一致必须403: %d",status)}
    if status:=send(http.MethodPost,trustedTestSecret,"village-1","boot-1",
        `{"gameId":"divine-beasts","playerId":"player-http","gameServerId":"village-1","serverBootId":"boot-1","sessionEpoch":8,"connectionId":"connection-9","leaseSeconds":20}`);status==200 {
        t.Fatalf("旧Epoch不能续租: %d",status)
    }
    if status:=send(http.MethodPost,trustedTestSecret,"village-1","boot-1",
        `{"gameId":"divine-beasts","playerId":"player-http","gameServerId":"village-1","serverBootId":"boot-1","sessionEpoch":9,"connectionId":"connection-9","leaseSeconds":31}`);status!=400 {
        t.Fatalf("续租上限30秒: %d",status)
    }
    if lease.calls!=0{t.Fatal("非法会话不能触发真实数据库续租")}
    if status:=send(http.MethodPost,trustedTestSecret,"village-1","boot-1",valid);status!=200 {
        t.Fatalf("当前真实租约可续期: %d",status)
    }
    if lease.calls!=1{t.Fatal("合法会话仅续租一次")}
}

func TestTrustedInventorySessionRenewNotOnPublicPlayerDataHandler(t *testing.T) {
    handler:=NewPlayerDataHandler(playerdata.NewService(playerdata.NewMemoryRepository()))
    req:=httptest.NewRequest(http.MethodPost,"/internal/v1/playerdata/inventory/session-renew",
        bytes.NewBufferString(`{}`))
    response:=httptest.NewRecorder()
    handler.ServeHTTP(response,req)
    if response.Code!=404 && response.Code!=405 {
        t.Fatalf("普通PlayerData不得暴露服务器租约续期: %d",response.Code)
    }
}
