# InventoryEntitlementProgressionIntegration（背包权益成长集成）

Inventory 商品使用现有 Inventory Service（背包服务）进行 Definition/Quantity 预检和幂等 Grant。购买 Quantity 会乘入 Inventory Reward 数量，并执行 int64/int（整数）上限保护。

Entitlement 商品第一版 Quantity 必须为 1，使用现有 Entitlement Definition 与幂等 Grant。

Progression XP 商品在 Commerce V1 明确 Unsupported；不会因为 Progression 插件已存在就自动开放“卖经验”。

跨域调用只发生在 PlayerData Commerce Application（玩家数据商城应用层），Commerce Domain 不 import Inventory/Entitlement/Progression Repository（背包/权益/成长仓储）。