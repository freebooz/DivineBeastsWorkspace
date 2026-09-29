# BackendDomain（后端领域）

Inventory（长期背包）领域真实位置为 `Backend/internal/modules/inventory`。`inventory.go` 定义 ContainerSnapshot（容器快照）、ItemInstance（物品实例）、QuickbarSlot（快捷栏槽位）、Snapshot（快照）、写命令、Repository Port（仓储端口）、Service（领域服务）以及与生产仓储语义一致的 MemoryRepository（内存仓储）。它不依赖 PostgreSQL、Gateway、DivineBeasts 项目代码或 UE 类型。

PlayerDataService 通过 `Backend/internal/modules/playerdata/inventory_bridge.go`持有最小 `InventoryService` 端口；这只是装配桥，不复制背包规则。没有新增独立 InventoryService 微服务，保持既定五服务架构。

生产 PostgreSQL 适配位于 `Backend/internal/platform/database/postgres/inventory_repository.go`，SQL 真源为 `Backend/migrations/000008_player_inventory.sql`。每个写操作在单个数据库事务内完成物品/快捷栏变化、InventoryRevision 递增与 OperationId 持久结果保存；同 OperationId 同请求返回原结果，不同请求拒绝。

Gateway 公网适配位于 `Backend/internal/app/gateway/inventory.go`。玩家身份由现有 `requireAuth` 从 AccessToken 解析，公网 DTO 不包含 playerId；Gateway 通过 `Backend/internal/transport/httpadapter/gateway_clients_inventory.go` 调 PlayerData internal HTTP，PlayerData 服务端路由位于 `playerdata_server_inventory.go`。

当前领域只实现已拥有物品的 Move/Swap、Split/Merge、Quickbar 与快照/幂等，不实现 Grant/Consume、PolicyCatalog、QuestRewardHandler、Pickup 或 Economy。生产 gRPC 协议源已补充 Inventory RPC，但生成绑定/Adapter 尚未完成。
