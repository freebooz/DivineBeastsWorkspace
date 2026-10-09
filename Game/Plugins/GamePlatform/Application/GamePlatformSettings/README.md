# GamePlatformSettings（游戏平台设置插件）

状态：**Client设备设置核心已实装；Runtime／Server／Editor通用框架已实装但当前无非测试生产Provider；真实 Editor／Client／Server 编译与运行验收待补证**。

> 当前工程物理层名称仍为 `GamePlatform（游戏平台基础层）`，对应方案中的 `GameFoundation（游戏基础层）` 概念。不得另建平行 `GameFoundation` 插件目录。

## 1. 插件定位

GamePlatformSettings 是跨游戏、跨项目复用的统一设置基础设施，负责：

- `Descriptor（设置描述）`、`Provider（提供者）`、`Registry（注册表）`。
- Client／Server 确定性配置分层解析。
- 类型、范围、平台、端侧和作用域校验。
- `Snapshot（只读快照）` 与批量 `ChangeSet（变化集合）`。
- User 层异步持久化和逐版本 `Migration（迁移）`。
- UE 原生 `UGameUserSettings（引擎用户设置）` 设备设置适配。
- Dedicated Server（专用服务器）INI／Environment／CommandLine 只读配置覆盖。
- Editor／CI Provider 与 Descriptor 静态校验。
- 轻量 Diagnostics（诊断）。

三层依赖保持：

`DivineBeasts（项目层） → MobaCommon（MOBA通用层） → GamePlatform（游戏平台基础层）`

平台层不反向依赖上层。

## 2. 四模块结构

- `GamePlatformSettingsRuntime（设置运行时核心）`：Runtime；Client／Server／Editor 均可加载。
- `GamePlatformSettingsClient（设置客户端适配）`：ClientOnly；只进入 Client／Editor。
- `GamePlatformSettingsServer（设置服务器适配）`：ServerOnly；只进入 Server／Editor。
- `GamePlatformSettingsEditor（设置编辑器校验）`：Editor；只进入 Editor。

所有模块保持单向依赖：Client／Server／Editor → Runtime → GamePlatformCore。

当前源码中没有非测试 `IGamePlatformSettingsProvider`。因此四模块结构可以验证框架边界，但 Runtime／Server 尚未承载真实产品 SettingId；在逐领域完成单一真源迁移前，不得把现有 Input／UI／Camera／SFX 偏好重复注册到 Settings User 层。

## 3. 明确不负责

GamePlatformSettings 不成为“万能设置管理器”：

- Input（输入）重绑、灵敏度、反转轴、Touch 参数：归 `GamePlatformInput`。
- UI（用户界面）文本缩放、高对比度、减少动画等实际应用：归 `GamePlatformUI`。
- Camera（相机）算法和镜头行为：归 `GamePlatformCamera`。
- SFX（音效）播放、Mix、Bus、SoundClass：归 `GamePlatformSFX`。
- Surface（环境表面材质）的积雪、苔藓、湿润、积水、全局天气材质状态与MPC桥接：归 `GamePlatformSurface`；Settings只能在存在真实用户偏好需求时保存画质／显示类选择，不能直接成为环境状态真源。
- 通用业务存档：归 `GamePlatformSave`。
- LiveOps（运营）动态活动、商品、奖励：归 `GamePlatformLiveOps`。
- Definition（定义资产）描述内容本身，不归 Settings。
- 神兽联盟专属 SettingId 和项目默认策略：由 DivineBeasts 层 Provider 注册。
- 服务器权威玩法、伤害、移动、准入、比赛规则：禁止通过设置系统修改。

## 4. 核心运行模型

### Runtime 通用设置

`Provider → Registry → Resolve → Validate → Snapshot → ChangeSet`

客户端解析层：

`PlatformDefault → ProjectDefault → ProviderDefault → User → Session`

服务器解析层：

`PlatformDefault → ProjectDefault → ProviderDefault → ServerDefault → Deployment → Environment → CommandLine → Session`

解析顺序由代码显式定义，不依赖模块加载或 TMap 遍历顺序。

### 客户端设备设置

`UGamePlatformDeviceSettingsSubsystem` 直接适配 UE5.8 `UGameUserSettings`：

`Stage → Preview/Commit → Confirm/Cancel`

Preview 不写盘；Confirm／Commit 才进行一次原生保存。

### User Profile

非图形稳定用户偏好可以由 `UGamePlatformUserSettingsProfile` 承载 Runtime 的 User 层纯值并使用 `AsyncSaveGameToSlot` 异步保存，但只有在原领域旧持久化完成迁移后才允许注册，避免双重真源。Profile 不保存认证、角色、背包、装备、任务、凭据、令牌、密钥或服务器业务状态。

### Server

服务器配置只读，不允许运行时回写部署配置：

`INI → Environment → CommandLine`

非法值、类型错误、端侧错误必须 Fail Closed（失败关闭）；敏感 Descriptor 禁止通过 Server INI／Environment／CommandLine 注入，必须使用部署秘密机制。

## 5. 性能与健壮性原则

- Runtime／Client／Server／Editor：**0 Tick / 0 Ticker**。
- Snapshot 查询通过 TMap，接近 O(1)。
- Provider 批量注册，避免运行时反射扫描。
- `SetValue` 只改内存层并标 Dirty；显式 `Apply` 批量解析与广播。
- User Profile 使用异步保存，Slider／ValueChanged 不直接写盘。
- ChangeSet 按批次广播，避免单字段事件风暴。
- UObject 只在游戏线程操作；异步保存完成后通过弱引用回到游戏线程。
- `bSensitive` 只代表诊断脱敏，不等于安全存储；敏感 Descriptor 只允许 Session 临时作用域，凭据／令牌／密钥禁止进入 Settings 持久层。
- Migration 在副本上执行，失败不覆盖原 Profile。

## 6. 组件清单

| 组件 | 中文名称 | 模块 | 核心职责 | 生命周期 | 主要调用方 | 端侧 |
| --- | --- | --- | --- | --- | --- | --- |
| `IGamePlatformSettingsService` | 统一设置服务接口 | Runtime | Get/Set/Reset/Apply/Save/Reload/Snapshot/订阅 | GameInstance | 项目组合层、领域消费者 | Client/Server |
| `IGamePlatformSettingsProvider` | 设置描述提供者接口 | Runtime | 允许上层注册自己的 Descriptor | 模块注册期 | MobaCommon、DivineBeasts | Client/Server |
| `IGamePlatformSettingsPersistenceProvider` | 设置持久化／部署提供者接口 | Runtime | 端侧持久层加载与保存适配 | 模块注册期 | Client/Server 适配模块 | Client/Server |
| `IGamePlatformSettingsMigration` | 设置版本迁移接口 | Runtime | 单步 Schema 迁移 | Reload 阶段 | 上层迁移实现 | Client |
| `FGamePlatformSettingDescriptor` | 设置描述 | Runtime | ID、类型、默认值、范围、作用域、端侧、平台、敏感性 | 值对象 | Provider／Registry | 通用 |
| `FGamePlatformSettingsRegistry` | 设置注册表 | Runtime Private | Provider 收集、冲突检测、近 O(1) 查询 | GameInstance | Runtime 子系统 | 通用 |
| `FGamePlatformSettingsResolver` | 设置分层解析器 | Runtime Private | 确定性覆盖与 Snapshot 生成 | Apply/Reload | Runtime 子系统 | 通用 |
| `FGamePlatformSettingsSnapshot` | 设置只读快照 | Runtime | 最终生效值、来源层、版本、Dirty 状态 | GameInstance | Gameplay/UI/领域适配 | 通用 |
| `UGamePlatformSettingsSubsystem` | 设置运行时子系统 | Runtime | Registry、解析、事件、迁移、保存编排 | GameInstance | 服务接口 | Client/Server |
| `UGamePlatformSettingsProjectSettings` | 工程级设置系统策略 | Runtime | 容量、Schema、严格模式、Profile 槽名 | Project Settings | 开发／CI | Editor/Runtime |
| `UGamePlatformDeviceSettingsSubsystem` | 客户端设备设置子系统 | Client | UGameUserSettings 暂存、预览、确认、回滚 | GameInstance | 设置页面 | Client |
| `UGamePlatformUserSettingsProfile` | 本地用户设置档案 | Client | User 层纯值持久化 | 本地存档 | Client Persistence Provider | Client |
| `FGamePlatformSettingsClientPersistenceProvider` | 客户端持久化提供者 | Client Private | Profile 读取与异步保存 | 模块生命周期 | Runtime | Client |
| `FGamePlatformSettingsServerPersistenceProvider` | 服务器部署配置提供者 | Server Private | INI／环境变量／命令行只读覆盖 | 模块生命周期 | Runtime | Server |
| `FGamePlatformSettingsEditorValidator` | 编辑器设置校验器 | Editor Private | Provider/Descriptor 冲突与合法性检查 | Editor 模块 | 开发／CI | Editor |

## 7. 文档

- `Docs/Architecture.md`：总体架构与边界。
- `Docs/API.md`：公开 API。
- `Docs/SettingsLifecycle.md`：设置生命周期。
- `Docs/ExtensionGuide.md`：MobaCommon／DivineBeasts 扩展示例。
- `Docs/Migration.md`：版本迁移。
- `Docs/PerformanceAndSecurity.md`：性能、安全与线程边界。
- `Docs/TestingAndEvidence.md`：自动化与真实验证证据。
- `Docs/ManualReview.md`：人工审核流程。
- `Docs/审查整改方案与执行计划.md`：本轮问题、方案、执行和剩余风险。



## 2026-09-30设计审查修订

进程注册的Client持久化对象只作工厂；每个GI通过CreateScopedProvider拥有独立用户上下文。旧Client自定义Provider未提供作用域克隆时返回SettingsScopedPersistenceRequired，不共享可变用户键。设备UGameUserSettings进程语义保留。ClientContext交错A/B真实保存回归源已补，未执行磁盘验收。

本次真实源码/Native/静态检查与未执行UE/后端/Cook边界见Game/Saved/Reviews/task2-repair-report.md；旧历史运行证据不自动覆盖本次修改。
