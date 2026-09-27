# InventoryBoundary（背包边界）

Hero/Skin/Feature 的长期解锁属于 Entitlement；Material（材料）、Consumable（消耗品）和 Equipment Instance（装备实例）属于 Inventory。

Entitlement 模块和后端领域不依赖 Inventory。一次 Quest Reward 可以分别产生 Inventory Grant 和 Entitlement Grant，但两个领域保持独立幂等、独立审计与独立事务边界。

禁止用“背包里存在某个虚拟物品”替代 Hero/Skin 权益。