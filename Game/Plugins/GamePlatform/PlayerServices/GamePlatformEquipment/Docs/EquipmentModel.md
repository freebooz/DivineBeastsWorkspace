# EquipmentModel（装备模型）

Persistent Equipment Snapshot（长期装备快照）按 CharacterId 保存 EquipmentRevision 与 Slots。Owner Slot 包含 ItemInstanceId；Public Snapshot 只复制 SlotId、EquipmentDefinitionId 和 VisualDefinitionId，不暴露私有 ItemInstanceId。

`UGamePlatformEquipmentComponent（平台装备组件）`复制 OwnerSnapshot、PublicSnapshot 与 EquipmentRuntimeGeneration。OwnerSnapshot 使用 OwnerOnly（仅拥有者）条件复制。