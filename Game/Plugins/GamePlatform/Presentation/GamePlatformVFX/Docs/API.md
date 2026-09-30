# API（公开接口）

`IGamePlatformVFXService（平台VFX服务接口）` 是低层客户端执行契约，所有接口仅允许 Game Thread（游戏线程）调用。

标准 Gameplay 不直接链接 VFX Client，而是：

```text
GamePlatformPresentation Request
→ ProviderChannel=VFX
→ DefinitionId
→ VFX Provider
→ IGamePlatformVFXService::Play
```

核心接口：

- `Play(Request)`：异步申请统一 Definition World Lease，正常返回 `Queued`；
- `Stop(Handle)`：停止 Pending/Active/Composite，并释放租约和定时器；
- `IsActive(Handle)`：查询当前 World 实例；
- `Preload(Request)` / `CancelPreload(Handle)`：使用 `GamePlatformData` World Lease 预持有 Definition 与 VFXRuntime Bundle；
- `RegisterCatalog/UnregisterCatalog`：仅历史低层工具兼容，不属于正式 Gameplay 路径。

`DefinitionId` 必须使用 `namespace.name@version` 的 GamePlatform 规范逻辑身份，不允许 `/Game/...` 资产路径。

预测语义：

```text
Predicted  → 播放
Confirmed  → 相同RequestId去重
Corrected  → 停止旧实例并使用纠正后的Definition/Transform重新播放
Cancelled  → 停止，不重播
```

Presentation Provider 传递 SourceLocation、TargetLocation、ImpactLocation、ImpactNormal；具体 `USceneComponent` 附着仍只属于低层 C++ 调用能力，在平台建立中立 Attachment 契约前不向 Presentation Core 塞项目对象引用。