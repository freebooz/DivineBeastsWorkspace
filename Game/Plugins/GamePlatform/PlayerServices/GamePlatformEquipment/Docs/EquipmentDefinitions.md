# EquipmentDefinitions（装备定义）

`UGamePlatformEquipmentDefinition（平台装备定义）`只包含 ServerSafe（服务器安全）字段：EquipmentDefinitionId、CompatibleItemDefinitionId、AllowedSlotIds、AbilitySetDefinitionIds、GameplayEffectDefinitionIds、GameplayTags、Version、VisualDefinitionId、RequirementId。

共享 Definition 禁止硬引用 StaticMesh、SkeletalMesh、Material、Niagara 或 AnimBP。

客户端 `UGamePlatformEquipmentVisualDefinition（装备视觉定义）`单独保存软 StaticMesh、SocketName、RelativeTransform、MaterialOverrides 与 PresentationTags。