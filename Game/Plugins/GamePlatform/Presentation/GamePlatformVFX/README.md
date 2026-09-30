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
→ IGamePlatformDataService::AcquireDefinition
→ UGamePlatformVFXDefinition : UGamePlatformDefinitionBase
→ VFXRuntime Asset Bundle
→ Niagara Executor
→ OnSystemFinished / Lifetime Timer
→ Release Definition Lease
```

当前实现要点：

- `GamePlatformVFXClient — ClientOnly（仅客户端）`，`GamePlatformVFXEditor — Editor（仅编辑器）`；
- `DBAClient（神兽联盟客户端组合插件）` 已显式启用 `GamePlatformVFX`；
- Definition 统一继承 `UGamePlatformDefinitionBase（平台定义基类）`，身份唯一来源为 `LogicalId`；
- 正式 Definition 加载使用 `IGamePlatformDataService` 的 World Lease（世界租约），不再使用私有 Definition 流式加载器；
- `GamePlatformPresentation` 是 SemanticTag/Context → ProviderChannel/DefinitionId 的唯一正式语义解析真源；
- VFX Catalog/Resolver 仅作为旧低层工具兼容入口，不属于标准 Gameplay 路径；
- Niagara Component 先完成参数/生命周期绑定，再激活运行；自然结束通过 `OnSystemFinished` 即时释放实例与 Definition Lease；
- `MaxLifetimeSeconds` 通过 Timer（定时器）真实执行，不依赖下一次 Play 才清理；
- `MaxActiveInstances` 是软预算，`HardMaxTrackedInstances` 是任何 Critical 请求都不能越过的绝对安全上限；
- Composite 子节点与普通请求共用预算门禁和 Definition Lease；
- Predicted / Confirmed / Corrected / Cancelled 与 Presentation 状态对齐；
- 非 Composite Behavior 统一以 Generic Niagara（通用 Niagara）执行，不建立十套播放器。Beam/Area/Attached 仅有少量通用参数/附着适配；Projectile/Shield/Portal/Trail/World 等名称主要是内容制作语义分类。

## 边界

平台层不得认识《神兽联盟》生肖、英雄、技能或 MOBA 规则。项目层只负责：

1. 创建 VFX Definition 内容实例；
2. 在 Presentation Catalog 注册项目语义 → VFX DefinitionId 映射；
3. 提供真实 Niagara/材质/纹理等客户端内容资产。

当前仓库仍没有真实 `.uasset/.umap` VFX 二进制资产。UE5.8 `GamePlatformVFXClient + GamePlatformVFXEditor` 定向模块编译已真实通过；Client Target 已通过 UHT 但 C++ 终态仍未取得，Automation 又被本机 VisionOS SDK 校验在测试执行前阻断。因此当前仍不能把源码/Editor编译结果替代 Client、Cook、Review Map、Multi-PIE 或 5v5/Android 性能验收。
