# ConsumeAndUseBoundary（消耗与使用边界）

长期 Inventory Consume（背包扣减）由 PlayerData/PostgreSQL 权威执行；必须验证本人 ItemInstance、Quantity、ItemPolicy.Consumable 和当前库存数量。Quantity 归零时删除实例并清理 Quickbar。

普通 Client 公共 Gateway 本轮不开放 Grant，也没有直接开放战斗中 Consumable（消耗品）权威使用链。影响实时 Combat（战斗）的物品使用必须由 UE Dedicated Server（专用服务器）参与验证后再执行后端 Reserve/Consume 与 GameplayEffect（玩法效果）。

因此“战斗中任意消耗品完整玩法”当前为不适用/未实现，不会出现 `Client → PlayerData Consume → Client自行加Buff`。
