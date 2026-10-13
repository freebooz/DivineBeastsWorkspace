//go:build productiondeps

package postgres

// 可信背包Outbox（事务发件箱）契约测试：下游物品效果只接受同时绑定
// 游戏、实例Boot、玩家连接世代、真实连接、物品实例及稳定操作号的事件。
// 本测试是事件结构校验，不模拟已发布NATS或已实际执行游戏效果。

import (
    "encoding/json"
    "testing"
)

func TestTrustedInventoryOutboxSchemaCarriesPlayerLease(t *testing.T) {
    event := inventoryOutboxEvent{
        SchemaVersion:2, GameID:"divine-beasts",
        SessionEpoch:9, ConnectionID:"connection-9",
        OperationID:"consume-11111111111111111111111111111111",
        SourceEventID:"use-evt-1", GameServerID:"village-1",
        ServerBootID:"boot-1", PlayerID:"player-a",
        ItemDefinitionID:"item.consumable.herbal_potion",
        ItemInstanceID:"instance-potion-42",
        Quantity:1, RemainingQuantity:0,
        ReasonCode:"item_use",
    }
    encoded,err:=json.Marshal(event)
    if err!=nil {t.Fatal(err)}
    var fields map[string]any
    if err=json.Unmarshal(encoded,&fields);err!=nil {t.Fatal(err)}
    for name, expected := range map[string]any{
        "schemaVersion":float64(2), "gameId":"divine-beasts",
        "sessionEpoch":float64(9), "connectionId":"connection-9",
        "sourceEventId":"use-evt-1",
        "gameServerId":"village-1", "serverBootId":"boot-1",
        "playerId":"player-a", "itemInstanceId":"instance-potion-42",
        "quantity":float64(1), "remainingQuantity":float64(0),
    } {
        if got,ok:=fields[name];!ok || got!=expected {
            t.Fatalf("Outbox事件字段未保留可信玩家/消耗身份: field=%s value=%v expected=%v",name,got,expected)
        }
    }
}
