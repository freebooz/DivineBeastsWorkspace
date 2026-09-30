# PresentationProviderIntegration（表现提供者集成）

正式链路：Gameplay Fact / GameplayCue / Application Fact → GamePlatformPresentation Semantic Request → ProviderChannel=VFX → `UGamePlatformVFXPresentationBridgeSubsystem` → `FGamePlatformVFXPresentationProvider` → `IGamePlatformVFXService`。

Bridge 为 `ULocalPlayerSubsystem`，向 `UGamePlatformPresentationClientSubsystem` 注册/注销 Provider。Provider 只消费平台中立字段，不引用 Moba/DivineBeasts 类型。

当前映射包括：

- RequestId → VFX RequestId/ActivationId；
- RequestGeneration → PredictionKey；
- DefinitionId 直接进入统一 Definition 加载；
- SourceLocation / TargetLocation / ImpactLocation / ImpactNormal；
- Priority → VFX Importance；
- Predicted / Confirmed / Corrected / Cancelled 完整映射。

Corrected 使用相同 RequestId 停止旧预测实例并重新播放纠正后的 Definition/位置。