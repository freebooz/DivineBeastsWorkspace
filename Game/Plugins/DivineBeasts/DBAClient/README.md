# DBAClient（神兽联盟客户端组合插件）

正式位置：`Game/Plugins/DivineBeasts/DBAClient/`。插件按模块宿主类型隔离客户端代码；虽然包含一个`Runtime`项目表现语义模块，服务器模块不得依赖任一`ClientOnly`模块。

| 模块 | 宿主 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsApplicationFlowClient` | 客户端 | 登录后角色创建/选择、项目准入适配和流程状态 |
| `DivineBeastsPresentationRuntime` | 双端 | 项目表现上下文、目录和内容包稳定语义 |
| `DivineBeastsPresentationClient` | 客户端 | 项目表现适配与平台注册 |
| `DivineBeastsUIClient` | 客户端 | 项目界面契约、路由和ViewModel |

插件直接声明模块所需的`DBAGameplay`与平台插件依赖，不依赖DBAArena、GamePlatformArena或MobaPresentation。竞技客户端模块已迁入DBAArena；它单向消费本插件的公开流程扩展，注册与注销仍按GameInstance作用域处理。通用流程执行器、平台UI与VFX播放实现不归本插件。

之前引用的`DivineBeastsApplicationContracts.generated.hpp`在仓库和生成目录均不存在，代码未使用其中声明；已移除该孤立include。后端请求继续走现存 Shared HTTP（共享HTTP）契约与真实HTTP适配代码，不恢复重复协议层。

### ApplicationFlow 当前正式状态（2026-09-27复核）

`DivineBeastsApplicationFlowClient（神兽联盟应用流程客户端）` 已完成到现行平台流程契约的源码迁移：项目层不再调用旧 `StartRun / TransitionTo / BeginOperation / IsOperationCurrent / InvalidateRun / RegisterNode / UnregisterNode / AllowedNextNodes`，而是通过 `RegisterNodeFactory（注册节点工厂） + UGamePlatformFlowDefinition（流程定义） + GamePlatformData Lease（数据租约） + StartFlow（启动流程） + SubmitEvent（提交事件） + FGamePlatformFlowNodeToken（流程节点令牌）` 使用唯一平台状态机。

项目层已增加 `UDivineBeastsApplicationFlowContext（神兽联盟应用流程上下文）` 和项目节点基类；上下文只保存跨节点稳定业务值，世界、Actor、Widget 不跨地图强持有，Endpoint 与 TransferTicket 仅在私有字段中短暂存在并在交给 Session 后清除。Authentication、CharacterEntry、WorldReady、InWorld 使用被动事件节点等待真实领域事件，其余步骤通过平台 Callback Flow Node（回调流程节点）执行；整个模块没有业务级 `Tick` 或 `FTSTicker`。

世界进入继续采用真实 Loading（加载）屏障：六项就绪事实绑定 ObservationId（观察身份），世界逻辑身份通过 Data 租约加载 `UDivineBeastsWorldDefinition（神兽联盟世界定义）`，再以 `MapIdentity（地图资产身份）` 校验当前实例真实世界。`MapId` 仍是后端/会话逻辑标识，不直接当磁盘地图包路径。Session 已提供公开 `BeginTransfer / CancelTransfer / ReportLocalFact / OnSessionChanged` 契约；客户端不得自行伪造 NetworkConnected 或 AdmissionConfirmed。

本轮真实验证：`Tests/Architecture/ValidateProjectHeaders.ps1` 检查 293 处自有头文件引用、0 缺失；`DivineBeastsApplicationFlowClient`、`GamePlatformUIClient` 与 `DivineBeastsUIClient` 已使用 UE5.8 客户端目标完成定向编译；平台 `GamePlatformApplicationFlow` 原生生产调度核心 Debug/Release 均为 `Cases=31 Failed=0`。完整 `DivineBeastsArenaClient` 构建仍被本任务范围外的主工程 Online/PCG 公开头依赖缺失，以及 `GamePlatformWorld` 自动化测试标志与 UE5.8 不兼容所阻断，因此不能据此宣称完整客户端或端到端世界进入已通过。

正式现行设计见 `Docs/ApplicationFlowArchitecture.md`；迁移前文档与旧描述快照继续保留在 `Docs/Legacy/`，不得作为恢复旧接口的依据。

### 当前用户界面设计

项目层用户界面当前设计基线见 `Docs/用户界面设计.md`。该文档定义了分类基础类继承、事件驱动更新、PC/移动端适配、目录规划、界面清单、命名规范和分阶段实施顺序。

P0 UI 底座已开始落地：GamePlatformUI 已新增普通/可激活分类基类、LocalPlayer 自适应子系统和 SafeZone 支持；DivineBeastsUIClient 已新增项目分类基类，并建立登录、真实加载、RootLayout 和五类 HUD 的 C++ / Blueprint 父类。ApplicationFlow 的 Blueprint `uint64` 反射阻断和 GamePlatformUIClient 生成代码错误已经消除；当前完整客户端构建的已知阻断位于主工程 Online/PCG 头依赖及 GamePlatformWorld 测试源码。UI 与 Flow 仍须保持事件驱动、禁止逐帧轮询。
