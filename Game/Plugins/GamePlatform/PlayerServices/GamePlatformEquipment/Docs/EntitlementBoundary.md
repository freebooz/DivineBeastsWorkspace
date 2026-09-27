# EntitlementBoundary（权益边界）

普通装备资格以 Inventory Ownership（背包所有权）和 Equipment Catalog（装备目录）为基础。

如果未来某装备还要求 Entitlement（权益），应由 PlayerData application（玩家数据应用层）组合 Equipment + Entitlement 校验；Equipment 领域不复制 Entitlement Grant，也不信任客户端自报 Entitled。

当前 Development Equipment（开发装备）没有 Entitlement Requirement；真实跨领域资格校验未执行。