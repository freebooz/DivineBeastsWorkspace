# Architecture（架构）

版本：0.2.0｜2026-09-30

`GamePlatformVFX` 位于 GamePlatform（游戏平台层）Presentation（表现）分类，只提供跨游戏通用 VFX 执行机制。

三层依赖保持：

```text
DivineBeasts（项目层） → MobaCommon（MOBA层） → GamePlatform（平台层）
```

正式代码模块只有：

- `GamePlatformVFXClient（VFX客户端运行模块）`：ClientOnly，负责 Definition 执行、World 生命周期、预算、预测、Niagara、Composite 和诊断；
- `GamePlatformVFXEditor（VFX编辑器模块）`：只负责 Definition/参数/LWC/FixedBounds/依赖等编辑器验证。

不创建 Runtime/Server 空模块。Dedicated Server 不加载客户端 VFX 模块。

## 单一真源

- `GamePlatformPresentation`：唯一负责 SemanticTag + Context → ProviderChannel + DefinitionId；
- `GamePlatformData`：唯一负责 Definition 逻辑身份、版本、依赖、World/Instance Lease；
- `GamePlatformVFX`：只负责 Definition 对应 Niagara 如何安全执行。

标准调用链：

```text
Gameplay Fact / GameplayCue / Application Fact
→ GamePlatformPresentation Catalog Resolve
→ ProviderChannel=VFX + DefinitionId
→ UGamePlatformVFXPresentationBridgeSubsystem
→ FGamePlatformVFXPresentationProvider
→ IGamePlatformVFXService
→ UGamePlatformVFXWorldSubsystem
→ IGamePlatformDataService::AcquireDefinition(World, VFXRuntime)
→ UGamePlatformVFXDefinition
→ Niagara Executor
→ Instance Registry / Handle
→ OnSystemFinished 或 Lifetime Timer
→ ReleaseDefinition
```

旧 `UGamePlatformVFXCatalog / Resolver` 只保留给历史低层工具：只有 Request 没有 `DefinitionId` 时才允许使用，不得恢复成 Gameplay 正式语义路径。

## 生命周期与预算

World 是实例、Definition Lease、Dedupe 和定时器的隔离边界。普通请求和 Composite Child 都必须经过同一 `ShouldSpawn` 预算门禁。`MaxActiveInstances` 是可被 Critical 突破的软预算；`HardMaxTrackedInstances` 是任何请求均不可突破的绝对上限。

Niagara Spawn 时禁止自动激活；平台先登记实例并绑定 `OnSystemFinished`，再调用 `Activate`。自然结束、主动 Stop、Corrected、Cancelled、MaxLifetime 和 World Deinitialize 均释放对应 Definition Lease。
