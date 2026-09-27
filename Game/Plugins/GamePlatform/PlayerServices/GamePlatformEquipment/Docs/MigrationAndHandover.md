# MigrationAndHandover（迁移与交接）

新增数据库变更仅使用 `Backend/migrations/gameplatform/0004_player_equipment.up.sql/down.sql`，不修改 Quest/Inventory/Entitlement 历史 Migration。

通用 Equipment API/Event Contract 位于 Shared/Contracts/GamePlatform；项目 Equipment Catalog Schema 位于 Shared/Contracts/Games/DivineBeasts。

后续 Progression（成长）如增加 RequiredLevel，应通过稳定 RequirementId/应用层组合，不在 Equipment 中创建假等级系统。下一插件仍是 GamePlatformProgression，本轮未进入。