# PersistenceAndReconciliation（持久化与对账）

长期真源位于 PlayerDataService + PostgreSQL。EquipmentServer 不直接访问数据库。

Persistence Port 支持 LoadEquipment、Equip、Unequip 和 QueryOperationResult。DBAServer 使用异步 FHttpModule（HTTP模块），完成回调切回 GameThread（游戏线程）。

EquipmentRevisionConflict 或 PersistenceOutcomeUnknown 会触发重新加载 Snapshot。跨服恢复依赖后端 Snapshot，不依赖旧服务器内存。