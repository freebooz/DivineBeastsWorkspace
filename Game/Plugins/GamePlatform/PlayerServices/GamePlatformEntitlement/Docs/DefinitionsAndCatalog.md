# DefinitionsAndCatalog（定义与目录）

平台 `UGamePlatformEntitlementDefinition（平台权益定义）`只保存稳定 EntitlementId、Category、TargetType、TargetId、Version、DisplayMetadataId 与 DefaultPolicy，不硬编码神兽联盟具体英雄名称。

Go 后端的 `DefinitionCatalog（定义目录）`是 Grant/Check 的服务器规则来源。PlayerData 启动必须提供 `ENTITLEMENT_DEFINITION_FILE（权益定义文件）`，缺失时 fail-fast（快速失败）。

开发目录位于 `Backend/configs/games/divinebeasts.entitlement.development.json`；Schema（结构约束）位于 `Shared/Contracts/Games/DivineBeasts/Schemas/entitlement-catalog.v1.schema.json`。