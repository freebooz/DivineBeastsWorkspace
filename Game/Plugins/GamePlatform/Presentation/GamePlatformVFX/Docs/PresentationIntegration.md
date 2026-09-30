# PresentationIntegration（表现层集成）

正式调用方向：

```text
Gameplay / GAS / Application Fact
→ GamePlatformPresentation
→ Catalog唯一解析 SemanticTag + Context
→ ProviderChannel=VFX + DefinitionId
→ VFX Provider
→ IGamePlatformVFXService
```

`GamePlatformPresentation` 已具备正式 Provider 注册和 Catalog 解析契约，VFX 不再重复维护正式语义 Resolver。

Provider 当前传递 RequestId、DefinitionId、ContextTags、Source/Target/Impact 位置、Priority 与 PredictionState。Attached 的具体 `USceneComponent` 仍属于低层 C++ 能力；在平台建立中立 Attachment Anchor 契约之前，不向 Presentation Core 引入项目 Actor/Component 类型。
