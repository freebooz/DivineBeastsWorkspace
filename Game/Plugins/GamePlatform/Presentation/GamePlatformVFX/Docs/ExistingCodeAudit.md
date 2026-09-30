# ExistingCodeAudit（现有代码审计）

版本：0.2.0｜2026-09-30

本轮在真实源码复审后完成以下收敛：

- `UGamePlatformVFXDefinition`：由独立 `UPrimaryDataAsset` 身份迁移为 `UGamePlatformDefinitionBase`；删除 VFX 自有 DefinitionId/Version/Revision 真源；
- Definition 加载：退休 `FGamePlatformVFXPreloadCoordinator`，正式运行和显式 Preload 统一使用 `IGamePlatformDataService::AcquireDefinition`；
- `FGamePlatformVFXResolver / Catalog`：从正式 Gameplay 语义链降为低层兼容能力；Presentation 已解析 DefinitionId 时完全绕过；
- `FGamePlatformVFXInstanceRegistry`：继续作为 World 私有实例索引；自然结束由 `OnSystemFinished` 触发即时回收，不再依赖下一次 Play 才 Prune；
- MaxLifetime：从“下次Prune顺便检查”改为真实 Timer Deadline；
- Budget：`MaxActiveInstances` 为软预算，新增 `HardMaxTrackedInstances` 绝对上限；Composite Child 同样执行预算门禁；
- Prediction：新增 Corrected 状态；
- Niagara：继续使用原生组件池和 Effect Type，不自研粒子线程、Pool 或 Culling 系统；
- 项目装配：`DBAClient.uplugin` 已显式启用 `GamePlatformVFX`。

当前仍保留 Catalog/Resolver 源码是为了旧工具兼容，不代表推荐新业务继续注册第二套 VFX 语义目录。