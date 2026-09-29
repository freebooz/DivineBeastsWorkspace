# MigrationAndHandover（迁移与交接）

本轮保持 `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory` 稳定身份和单一 `GamePlatformInventoryClient` UE 模块，没有迁移到 Gameplay、没有建立 `GamePlatformInventoryServer`、没有复制第二套背包插件。

原 `Backend/internal/modules/inventory/doc.go`占位包已经在原位置增量实现为真实领域，新增 `inventory.go` 与 `inventory_test.go`；PlayerData 通过 `inventory_bridge.go`装配该领域。生产持久化新增 `Backend/internal/platform/database/postgres/inventory_repository.go`，数据库 Migration 为 `000008_player_inventory.sql`。没有新增微服务。

Shared 新增真实公网契约 `Shared/Contracts/GamePlatform/OpenAPI/inventory.openapi.yaml`；Go 内部 `player-data-service.proto`新增 Inventory RPC 协议源。由于当前 Runner 缺少 Codegen 工具，Generated 目录没有手工修改，正式生成绑定仍待工具链恢复后执行。

旧文档曾声明的 `Backend/gameplatform/inventory`、`0002_player_inventory`、Grant/Consume Outbox、Quest Reward Map、External Pickup 和 Item Policy 文件在当前工程中不存在或未实现，本轮已统一纠正为真实状态。

Equipment、Economy、Entitlement 继续独立：Inventory 只回答“长期持有哪些物品以及容器/槽位位置”，不负责装备运行效果、货币余额或永久权益。
