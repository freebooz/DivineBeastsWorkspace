# GamePlatformVFX（游戏平台视觉特效插件）

版本：0.2.0｜2026-09-30

`GamePlatformVFX` 是 GamePlatform（游戏平台层）的跨游戏客户端 VFX 执行基础设施。它消费 `GamePlatformPresentation` 已解析的中立表现请求，不拥有 Gameplay 事实，也不维护第二套项目语义真源。

## 当前正式架构

```text
Gameplay / Application Fact
→ GamePlatformPresentation
→ ProviderChannel=VFX + DefinitionId
→ GamePlatformVFX Presentation Provider
→ IGamePlatformVFXService
→ UGamePlatformVFXWorldSubsystem
→ World共享 Definition Cache
→ IGamePlatformDataService::AcquireDefinition（首次缓存未命中）
→ UGamePlatformVFXDefinition : UGamePlatformDefinitionBase
→ VFXRuntime Asset Bundle
→ Niagara Executor
→ OnSystemFinished / Lifetime Timer
→ 实例使用计数归零后保留有界热缓存
→ LRU淘汰或World Deinitialize时 ReleaseDefinition
```

当前实现要点：

- `GamePlatformVFXClient — ClientOnly（仅客户端）`，`GamePlatformVFXEditor — Editor（仅编辑器）`；
- `DBAClient（神兽联盟客户端组合插件）` 已显式启用 `GamePlatformVFX`；
- Definition 统一继承 `UGamePlatformDefinitionBase（平台定义基类）`，身份唯一来源为 `LogicalId`；
- 同一个 Definition 在同一 World 只申请一份共享 Data Lease；并发 Play / Preload 复用该 Lease，空闲项按 LRU（最近最少使用）在 `MaxCachedDefinitions` 容量内保留；
- `GamePlatformPresentation` 是 SemanticTag/Context → ProviderChannel/DefinitionId 的唯一正式语义解析真源；
- VFX Catalog/Resolver 仅作为旧低层工具兼容入口，不属于标准 Gameplay 路径；
- Niagara Component 建立 Component→Handle 反向索引；自然结束不再扫描全部实例；
- Dedupe 同时维护 Key→Handle 与 Handle→Key；Play 热路径不再全表 Prune；
- Composite 延迟步骤 Timer 归 Parent Handle 管理，Stop / Corrected / Cancelled / World Deinitialize 都会取消；
- `MaxLifetimeSeconds` 通过 Timer（定时器）真实执行，不依赖下一次 Play 才清理；
- `MaxAmbientInstances / MaxStatusInstances / MaxActiveInstances / HardMaxTrackedInstances` 形成分层预算，低优先级不能占满 Combat（战斗）保留容量；
- Critical（关键）只提高裁剪优先级，不再被强制排除出 Niagara 原生 Pool（池）；
- Definition 的 EffectType 与 Default/Platform/Quality Niagara System EffectType 由 Editor Validator 强制一致；
- Definition 结构校验在共享缓存首次加载时执行一次；每次执行只校验动态参数；
- Spawn 默认参数与 Request 覆盖参数分两次写入 Niagara，避免每次合并复制六类参数 Map；
- 提供 `stat GamePlatformVFX` 计数与 CPU Trace Scope，覆盖请求、拒绝、Dedupe、共享Definition缓存、Composite Child、Tracked/Pending/Peak；
- Predicted / Confirmed / Corrected / Cancelled 与 Presentation 状态对齐；
- 非 Composite Behavior 统一以 Generic Niagara（通用 Niagara）执行，不建立十套播放器。Beam/Area/Attached 仅有少量通用参数/附着适配；
- 已启动 `F:\\VFX Lib` 第一批平台化迁移：通用 Shader 位于 `Shaders/Private/GamePlatformVFXCommonMotion.ush`，源美术只进入 `SourceArt`，不会冒充运行时 `.uasset`。

## 边界

平台层不得认识《神兽联盟》生肖、英雄、技能或 MOBA 规则。项目层只负责：

1. 创建 VFX Definition 内容实例；
2. 在 Presentation Catalog 注册项目语义 → VFX DefinitionId 映射；
3. 提供真实 Niagara/材质/纹理等客户端内容资产。

迁移说明见 `Docs/VFXLibMigration.md（VFX Lib迁移说明）`。

当前仓库仍没有真实 `.uasset/.umap` VFX 二进制资产。UE5.8 `GamePlatformVFXClient + GamePlatformVFXEditor` 性能整改后的定向模块编译已真实通过；但真实 1v1/5v5/OpenWorld/Village、Android Niagara Insights、Cook/Stage 与 Review Map 性能证据仍未取得，因此不能把源码和 Editor 编译结果描述为 Production Ready（生产就绪）。


## 2026-09-30 设计审查修复

本次资源/生命周期与行为合同见 [设计修复说明](Docs/DesignRemediation-2026-09-30.md)。源码及新增回归不等于UE运行、真实资产或Cook验收；准确执行证据由任务修复报告记录。


2026-10-09本插件源码整改、中文API/所有权说明和待UE验收边界见 [本轮源码说明](Docs/AuditRemediation-2026-10-09.md)。
