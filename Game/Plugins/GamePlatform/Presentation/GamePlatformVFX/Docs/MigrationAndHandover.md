# MigrationAndHandover（迁移与交接）

0.2.0 迁移目标是消除 VFX 与平台统一 Presentation/Data 架构的重复真源。

## 已完成迁移

- `UGamePlatformVFXDefinition : UGamePlatformDefinitionBase`；逻辑身份改用 `LogicalId`；
- 删除 VFX 私有 Definition 流式预加载器，改用 `GamePlatformData` World Lease；
- Presentation 已提供 `DefinitionId` 时不再进入 VFX Catalog/Resolver；
- 旧 Catalog API 保留兼容，但不得作为新 Gameplay 标准；
- Composite 子节点由软 Definition 对象引用改为规范逻辑 `DefinitionId`；
- 自定义 Pool 继续由 Niagara 原生 Pool 替代；
- 生命周期以 `OnSystemFinished + Timer` 事件驱动；
- `DBAClient` 作为项目客户端组合根显式启用 VFX 插件。

## 内容迁移要求

目前仓库没有真实 VFX `.uasset`，因此没有二进制资产需要自动迁移。未来新建 Definition 必须：

1. 配置 `LogicalId = namespace.name@version`；
2. 使用 `DataVersion` 而不是 VFX 自有 Version/Revision；
3. 将运行资源通过 `VFXRuntime` Bundle 持有；
4. Composite Step 填写子 `DefinitionId`，不保存直接软 Definition 真源；
5. 项目语义映射注册在 `GamePlatformPresentation Catalog`。

上层 DivineBeastsPresentation 只生产中立 Presentation Request 和项目 Catalog/Content Pack，不复制 WorldSubsystem、Data Loader、Pool 或 VFX Resolver。