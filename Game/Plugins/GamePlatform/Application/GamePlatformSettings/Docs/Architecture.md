# GamePlatformSettings 架构与职责

## 1. 架构定位

当前工程正式物理层名称为 `GamePlatform（游戏平台基础层）`；它对应外部方案中 `GameFoundation（游戏基础层）` 的概念职责，但不得因此恢复第二套目录。

依赖方向固定：

`DivineBeasts（项目层） → MobaCommon（MOBA通用层） → GamePlatform（游戏平台基础层）`

GamePlatformSettings 只依赖 Unreal Engine 基础模块与 `GamePlatformCore（平台核心契约）`。它不直接依赖 Input、UI、Camera、SFX、Online、Session、Telemetry、MobaCommon 或 DivineBeasts。

## 2. 为什么保留独立插件

审查前它只有一个 ClientOnly 模块入口，属于空壳。专项审查后确认“统一设置基础设施”具备独立跨项目价值：

- 上层插件需要稳定 Provider 扩展点，不应修改平台代码。
- 客户端和服务器都需要确定性覆盖、校验、Snapshot 和诊断。
- User 层设置需要版本迁移与持久化编排。
- `UGameUserSettings` 的显示／画质设置存在预览、确认与回滚事务。
- Dedicated Server 需要最小依赖的部署配置解析。
- Editor/CI 需要在 Cook/发布前发现重复 SettingId、Provider 冲突和端侧错误。

因此保留插件身份并拆分四个职责真实的模块。

## 3. 模块依赖

```text
GamePlatformSettingsEditor ─┐
GamePlatformSettingsServer ─┼→ GamePlatformSettingsRuntime → GamePlatformCore
GamePlatformSettingsClient ─┘
```

- Runtime：不包含 UMG／Slate／EnhancedInput／Niagara／AudioMixer，也不直接读 GConfig、UGameUserSettings 或 SaveGame。
- Client：可以使用 UGameUserSettings、SaveGame，但不依赖 Input/UI/Camera/SFX 实现。
- Server：可以读 GConfig、Environment、CommandLine，但不包含客户端表现与输入。
- Editor：只消费 Runtime 公开验证入口，不依赖 Client/Server 私有实现。

## 4. Provider 与 Registry

`IGamePlatformSettingsProvider` 是上层唯一正式扩展入口。Provider 批量返回自己的 Descriptor。

Registry 重建时：

1. 获取已注册 Provider。
2. 按 ProviderId 稳定排序。
3. 限制 Provider/Descriptor 数量。
4. 过滤当前平台、端侧和 DevelopmentOnly。
5. 校验 Descriptor。
6. 拒绝重复 ProviderId 和重复 SettingId。
7. 生成 `TMap<SettingId, Descriptor>` 与所有者映射。

冲突必须失败，不按加载顺序决胜。

## 5. Descriptor 类型模型

`FGamePlatformSettingDescriptor` 包含：

- SettingId（设置唯一标识）
- Category（分类）
- ValueType（类型）
- DefaultValue / DefaultLayer（默认值／默认层）
- Minimum / Maximum（范围）
- PersistenceScope（持久化作用域）
- RuntimeScope（运行端侧）
- ApplyMode（应用方式）
- PlatformAllowList（平台限制）
- RestartRequired（重启要求）
- Sensitive（敏感）
- DevelopmentOnly（开发专用）
- SchemaVersion（结构版本）

设置值使用 `FGamePlatformSettingValue` 类型化字段，不把全部值退化为 FString。

## 6. 分层解析

客户端：

`PlatformDefault → ProjectDefault → ProviderDefault → User → Session`

服务器：

`PlatformDefault → ProjectDefault → ProviderDefault → ServerDefault → Deployment → Environment → CommandLine → Session`

解析器对每个候选值再次执行类型／范围／端侧／作用域校验。Snapshot 记录最终值及 SourceLayer，方便诊断“为什么被覆盖”。

## 7. Snapshot 与事件

`FGamePlatformSettingsSnapshot` 是运行时唯一公开只读视图，内部使用 TMap 进行接近 O(1) 查询。

`FGamePlatformSettingsChangeSet` 一次表示一个 Apply／Reset／Reload／Migration／Persistence／ProviderChanged 批次。消费者通过订阅获取批量变化，不为每个 Slider 值制造无序事件风暴。

## 8. Persistence 与 Migration

Runtime 只依赖抽象 Persistence Provider：

- Client：`UGamePlatformUserSettingsProfile` + `AsyncSaveGameToSlot`。
- Server：INI／Environment／CommandLine，只读不回写。

`IGamePlatformSettingsMigration` 只允许单步 `Vn → Vn+1`。Migration Runner 在工作副本上执行，完整成功后才提交；失败保留原内存值和原文件。

## 9. 设备设置边界

客户端设备级显示和 Scalability 不经过通用字符串层反复解析，而由 `UGamePlatformDeviceSettingsSubsystem` 直接适配 UE5.8 `UGameUserSettings`，提供：

`Stage → Preview/Commit → Confirm/Cancel`

这条路径与 User Profile 的“业务偏好 User 层”分开，避免重复实现 Unreal 自带图形设置系统。

## 10. 生命周期与线程

Runtime 使用 GameInstance 生命周期，跨地图保持，不依赖单一 World。没有 Tick/Ticker。

所有 UObject 操作在游戏线程。异步保存只在 IO 阶段离开当前同步流程；完成回调使用弱子系统并回到游戏线程后更新 Snapshot。

## 11. 网络与权威

Settings 只配置稳定系统／用户／服务器部署偏好。它不提供 RPC，不允许客户端通过设置修改服务器权威移动、技能、伤害、准入、经济或比赛结果。

服务器 Settings 只作为启动／部署配置真源之一；具体 ServerRole、容量、准入等已经由 GamePlatformServer/DBAServer 拥有的权威职责不能被重复迁入 Settings。

