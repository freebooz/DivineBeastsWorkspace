# GamePlatformInventory（游戏平台背包插件）

正式稳定路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory`。该插件属于 GamePlatform（平台层）PlayerServices（玩家服务适配），UE 侧只保留 `GamePlatformInventoryClient（背包客户端模块）`，Type=`ClientOnly（仅客户端）`；长期背包权威位于 `PlayerDataService（玩家数据服务） + PostgreSQL（关系数据库）`。不创建第二套 UE Server 背包真源。

当前已实现的客户端能力包括 Snapshot/Revision（快照/修订号）、Container/Slot（容器/槽位）、ItemInstance（物品实例）、Move/Swap、Split/Merge、Quickbar、Pending Operation、OperationId、ExpectedRevision、RevisionConflict 全量对账、AccountGeneration 与 SnapshotRequestGeneration 迟到响应隔离、派生排序/索引缓存和中立 ViewModel。默认 Transport 已改为复用 `GamePlatformOnlineClient（平台在线客户端）`的认证请求通道，不保存 AccessToken，也不自行实现 Token 刷新。

当前已实现的后端 HTTP 链为：`Gateway /v1/inventory... → PlayerData internal HTTP → Backend/internal/modules/inventory → PostgreSQL`。实际源码位于 `Backend/internal/modules/inventory/inventory.go`、`Backend/internal/platform/database/postgres/inventory_repository.go`，数据库真源为 `Backend/migrations/000008_player_inventory.sql`。OperationId 同键同内容支持持久幂等重放，同键异内容拒绝；背包状态、聚合 Revision 与 Operation 结果在同一 PostgreSQL 事务提交。

当前尚未实现可信 Dedicated Server 的 Grant/Consume（授予/消耗）、Quest Reward（任务奖励）、External Pickup（外部拾取）以及对应 Outbox 事件；这些能力不得由普通客户端 API 代替。生产 gRPC 的 `player-data-service.proto` 已增加 Inventory RPC 协议源，但当前 Runner 缺少 Go/Protobuf/OpenAPI 代码生成工具，因此生成绑定和 gRPC Inventory Adapter 仍待正式 Codegen。Equipment（装备）、Entitlement（权益）、Economy（经济钱包）继续保持独立领域。

验证状态以源码和实际工具执行为准：当前 Runner 无 `go/protoc/protoc-gen-go/protoc-gen-go-grpc/oapi-codegen`，因此 Go Test、Proto/OpenAPI Codegen 和生成物新鲜度尚未执行；UE 模块构建需在当前并行 UBT 构建释放全局互斥后再次验证。详见 `Docs/审查整改执行计划.md`。

