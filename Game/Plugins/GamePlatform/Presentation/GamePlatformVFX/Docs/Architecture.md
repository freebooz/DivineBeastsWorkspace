# Architecture（架构）

版本：0.2.0｜2026-09-30

`GamePlatformVFX` 位于 GamePlatform（游戏平台层）Presentation（表现）分类，只提供跨游戏通用 VFX 执行机制。

三层依赖保持：

```text
DivineBeasts（项目层） → MobaCommon（MOBA层） → GamePlatform（平台层）
```

正式代码模块只有：

- `GamePlatformVFXClient（VFX客户端运行模块）`：ClientOnly，负责 Definition 执行、World 生命周期、预算、预测、Niagara、Composite、共享Definition缓存和诊断；
- `GamePlatformVFXEditor（VFX编辑器模块）`：只负责 Definition/参数/LWC/FixedBounds/EffectType/依赖等编辑器验证。

不创建 Runtime/Server 空模块。Dedicated Server 不加载客户端 VFX 模块。

## 通用技术资产层

`GamePlatformVFXClient` 可以拥有跨游戏中立的 Shader/HLSL（着色器数学函数）、Niagara 参数名契约和未来的技术模板，但不得编码 FrostMage、PetalBloom、生肖、英雄或 Ability（技能）语义。`SourceArt` 只保存迁移源素材与追溯哈希，不属于运行时 `Content`。

## 单一真源

- `GamePlatformPresentation`：唯一负责 SemanticTag + Context → ProviderChannel + DefinitionId；
- `GamePlatformData`：唯一负责 Definition 逻辑身份、版本、依赖和 Lease；
- `GamePlatformVFX`：只负责 Definition 对应 Niagara 如何安全执行，并在 World 内共享 Data Lease。

标准调用链：

```text
Gameplay Fact / GameplayCue / Application Fact
→ GamePlatformPresentation Catalog Resolve
→ ProviderChannel=VFX + DefinitionId
→ UGamePlatformVFXPresentationBridgeSubsystem
→ FGamePlatformVFXPresentationProvider
→ IGamePlatformVFXService
→ UGamePlatformVFXWorldSubsystem
→ DefinitionCache 命中：直接执行
→ DefinitionCache 未命中：IGamePlatformDataService::AcquireDefinition(World, VFXRuntime)
→ UGamePlatformVFXDefinition 首次结构校验
→ Niagara Executor
→ Instance Registry / Handle
→ OnSystemFinished 或 Lifetime Timer
→ 实例使用计数回收
→ LRU 淘汰或 World Deinitialize 时 ReleaseDefinition
```

旧 `UGamePlatformVFXCatalog / Resolver` 只保留给历史低层工具：只有 Request 没有 `DefinitionId` 时才允许使用。

## 生命周期与预算

World 是实例、共享 Definition Lease、Dedupe 和 Timer 的隔离边界。普通请求和 Composite Child 都经过同一预算门禁。

- Component→Handle 与 Handle→DedupeKey 都是反向索引，完成/取消不扫描全表；
- Composite 延迟 Timer 归父 Handle 所有，父实例 Stop/Corrected/Cancelled 时立即清除；
- Niagara Spawn 时禁止自动激活；平台先登记实例和完成回调，再 Activate；
- 自然结束、主动 Stop、Corrected、Cancelled、MaxLifetime 只回收实例使用计数；共享 Definition Lease 由有界 World Cache 复用；
- World Deinitialize 清理全部实例、Timer和共享 Definition Lease。
