# ExistingCodeAudit（现有代码审计）

正式Owner为 Game/Plugins/GamePlatform/Presentation/GamePlatformVFX。未发现 Frontend/Plugins/GamePlatformClient 历史插件。UE5.8编译/UHT证据当前未执行。

127 UGamePlatformVFXSubsystem：Replace；正式Owner迁为 UGamePlatformVFXWorldSubsystem。
128 UGamePlatformVFXDefinition：Keep并生产化；补DefinitionId、PrimaryAsset、Behavior、Category、EffectType、Schema、变体、Lease/LWC/Lifetime/Version。
129 FGamePlatformVFXRequest：Keep并加固；补RequestId、ActivationId、PredictionKey、DefinitionId、ContextTags、Platform/Quality/Prediction。
130 FGamePlatformVFXHandle：Keep并加固；Id + Generation + 弱World身份。
131 FGamePlatformVFXSpawnContext：Keep；仅中立Transform/Attach。
132 FGamePlatformVFXRegistry：Migrate到World私有 FGamePlatformVFXInstanceRegistry。
133 FGamePlatformVFXResolver：Keep并生产化；确定性解析、歧义拒绝、revision-aware cache。
134 FGamePlatformVFXPool：Replace；使用Niagara原生组件池。
135 FGamePlatformVFXStreamingManager：Replace；迁到GamePlatformData FGamePlatformAssetLoader + Lease。
136 FGamePlatformVFXBudgetManager：Migrate；仅保留紧急总量上限/重要度桥接。
137 UGamePlatformVFXCullingPolicy：Replace；常规culling交Niagara Effect Type。
138 UGamePlatformVFXDebugService：Migrate到Diagnostics/DeveloperTools只读摘要。