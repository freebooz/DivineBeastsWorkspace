# PreloadingAssetLeases（预加载与资产租约）

0.2.0 起，VFX Definition 不再通过私有 `FGamePlatformVFXPreloadCoordinator` 或普通 `FGamePlatformAssetLoader` 加载。

正式流程：

```text
DefinitionId
→ GamePlatformDefinition PrimaryAssetId
→ IGamePlatformDataService::AcquireDefinition
→ Lifetime = World
→ Bundle = VFXRuntime
→ UGamePlatformVFXDefinition
```

`UGamePlatformVFXDefinition` 的 NiagaraSystem、EffectType、平台/质量变体和 PreloadAssets 均声明 `VFXRuntime` Asset Bundle，由统一 Data Lease 持有。实例自然结束、Stop、取消、MaxLifetime 或 World 销毁时调用 `ReleaseDefinition`。

显式 `Preload` 同样使用 World Lease，不建立第二套 Streamable Definition 租约。

`FGamePlatformAssetLoader` 目前只保留用于旧 Startup Catalog 普通软资产兼容加载；Catalog 不是标准 Gameplay 语义真源。未执行真实 Cook 前，不能把 Asset Bundle 元数据存在等同于资源已正确入包。