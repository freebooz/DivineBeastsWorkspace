# MigrationAndHandover（迁移与交接）

新增数据库变更只使用 `Backend/migrations/gameplatform/0003_player_entitlement.up.sql/down.sql`，不修改 0001 Quest 或 0002 Inventory 历史 Migration。

项目具体 Entitlement Catalog 与 Quest Reward Map 位于 Backend/configs/games 和 Shared/Contracts/Games/DivineBeasts；平台通用 API/Event Contract 位于 Shared/Contracts/GamePlatform。

后续接真实 Hero/Skin 流程时只需要映射 RequiredEntitlementId 并调用 DBAServer 授权服务，不应把项目英雄名称反向写入 Foundation 模块。