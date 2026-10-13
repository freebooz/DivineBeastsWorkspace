package httpadapter

// 可信游戏服事件额外安全门禁：方法、媒体类型及请求体长度在领域写入前拒绝。
// 仅模拟已经经过服务器验证的授权端口；本测试不代表正式数据库迁移和联机通过。
import (
    "bytes"
    "context"
    "encoding/json"
    "net/http"
    "net/http/httptest"
    "strings"
    "testing"

    "divinebeasts/backend/internal/modules/inventory"
)

func TestTrustedInventoryEventRejectsWrongMethodsAndMediaTypes(t *testing.T) {
    journal := &trustedHTTPJournal{}
    mutation := &trustedHTTPMutations{}
    processor, err := inventory.NewTrustedEventProcessor(
        "divine-beasts", trustedHTTPVerifier{}, journal, mutation)
    if err != nil { t.Fatal(err) }
    handler := NewTrustedInventoryEventHandler(processor, trustedTestSecret)
    event := inventory.ConfirmedInventoryEvent{
        EventKind: inventory.TrustedEventGrant, GameID: "divine-beasts",
        GameServerID: "village-1", ServerBootID: "boot-1",
        PlayerID: "player-http", SessionEpoch: 9, ConnectionID: "connection-9",
        SourceEventID: "verified-pickup-42", ExpectedRevision: 1,
        ItemDefinitionID: "item.herb.peach", Quantity: 2, ReasonCode: "pickup",
    }
    body, err := json.Marshal(event)
    if err != nil { t.Fatal(err) }
    send := func(method, mediaType string, payload []byte) int {
        t.Helper()
        req := httptest.NewRequestWithContext(context.Background(), method,
            "/internal/v1/playerdata/inventory/trusted-events", bytes.NewReader(payload))
        req.Header.Set("Authorization", "Bearer "+trustedTestSecret)
        req.Header.Set("X-Game-Server-Id", "village-1")
        req.Header.Set("X-Game-Server-Boot-Id", "boot-1")
        req.Header.Set("Content-Type", mediaType)
        res := httptest.NewRecorder()
        handler.ServeHTTP(res, req)
        return res.Code
    }

    if code := send(http.MethodGet, "application/json", body); code != http.StatusMethodNotAllowed {
        t.Fatalf("只允许POST可信事件: %d",code)
    }
    for _, media := range []string{
        "application/json-malformed", "text/plain", "", "application/json; charset=",
    } {
        if code := send(http.MethodPost, media, body); code != http.StatusUnsupportedMediaType {
            t.Fatalf("错误媒体类型必须拒绝: %q %d",media,code)
        }
    }
    huge := []byte(strings.Repeat("x", 8193))
    if code := send(http.MethodPost, "application/json", huge); code != http.StatusBadRequest {
        t.Fatalf("超长事件体必须拒绝: %d",code)
    }
    // 标准json.Decoder默认接受同一对象中的重复键；可信事件必须拒绝，防止身份歧义。
    duplicateKey := bytes.Replace(body, []byte(`"playerId":"player-http"`),
        []byte(`"playerId":"player-http","playerId":"another-account"`), 1)
    if bytes.Equal(duplicateKey, body) {
        t.Fatal("测试对象缺少playerId原始字段，无法验证重复键保护")
    }
    if code := send(http.MethodPost, "application/json", duplicateKey); code != http.StatusBadRequest {
        t.Fatalf("重复玩家ID字段必须拒绝: %d", code)
    }
    nullID := bytes.Replace(body, []byte(`"gameId":"divine-beasts"`),
        []byte(`"gameId":null`), 1)
    if code := send(http.MethodPost, "application/json", nullID); code != http.StatusBadRequest {
        t.Fatalf("null必填游戏身份必须拒绝: %d", code)
    }
    unknown := bytes.Replace(body, []byte(`"reasonCode":"pickup"`),
        []byte(`"reasonCode":"pickup","adminGrant":true`), 1)
    if code := send(http.MethodPost, "application/json", unknown); code != http.StatusBadRequest {
        t.Fatalf("未知管理员发奖字段必须拒绝: %d", code)
    }
    if journal.count != 0 || mutation.count != 0 {
        t.Fatalf("校验失败不得进入日志或库存事务: log=%d mutation=%d",journal.count,mutation.count)
    }
    if code := send(http.MethodPost, "application/json; charset=utf-8", body); code != http.StatusOK {
        t.Fatalf("合法JSON媒体类型应进入受控事件逻辑: %d",code)
    }
    if journal.count != 1 || mutation.count != 1 {
        t.Fatalf("合法事件应仅经一次日志和修改: log=%d mutation=%d",journal.count,mutation.count)
    }
}
