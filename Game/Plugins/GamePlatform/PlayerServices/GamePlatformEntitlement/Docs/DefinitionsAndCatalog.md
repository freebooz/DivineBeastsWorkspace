# DefinitionsAndCatalog（定义与目录）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

平台 `UGamePlatformEntitlementDefinition（平台权益定义）`只保存稳定 EntitlementId、Category、TargetType、TargetId、Version、DisplayMetadataId 与 DefaultPolicy，不硬编码神兽联盟具体英雄名称。

Go 后端的 `DefinitionCatalog（定义目录）`是 Grant/Check 的服务器规则来源。PlayerData 启动必须提供 `ENTITLEMENT_DEFINITION_FILE（权益定义文件）`，缺失时 fail-fast（快速失败）。

开发目录位于 `Backend/configs/games/divinebeasts.entitlement.development.json`；Schema（结构约束）位于 `Shared/Contracts/Games/DivineBeasts/Schemas/entitlement-catalog.v1.schema.json`。
