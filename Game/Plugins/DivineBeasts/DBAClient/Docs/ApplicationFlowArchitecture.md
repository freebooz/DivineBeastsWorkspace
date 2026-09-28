# 神兽联盟应用流程正式架构

版本：1.0.0｜更新日期：2026-09-27。

## 1. 现行唯一流程边界

`GamePlatformApplicationFlow（游戏平台应用流程）` 是唯一流程状态机，负责 Flow Run（流程运行）、NodeId（节点身份）、NodeGeneration（节点代次）、超时、重试、循环、取消和终态。`DivineBeastsApplicationFlowClient（神兽联盟应用流程客户端）` 只是项目组合根和业务节点提供者，不维护第二套 `CurrentState（当前状态）`，也不允许业务代码手工跳转节点。

正式调用链：

```text
GamePlatformData AcquireDefinition（取得流程定义租约）
    ↓
UGamePlatformFlowDefinition（流程定义）
    ↓
RegisterNodeFactory（注册节点工厂）
    ↓
StartFlow（启动唯一流程）
    ↓
UGamePlatformFlowNode / UGamePlatformCallbackFlowNode（流程节点）
    ↓
Completion 或 SubmitEvent（完成回调或提交事件）
    ↓
平台执行器根据 Definition 路由
```

旧 `StartRun / TransitionTo / BeginOperation / IsOperationCurrent / InvalidateRun / RegisterNode / UnregisterNode / AllowedNextNodes` 已退出正式源码，`Docs/Legacy` 中的同名设计仅作历史证据。

## 2. 项目上下文与数据所有权

`UDivineBeastsApplicationFlowContext（神兽联盟应用流程上下文）` 的 Outer（外部所有者）必须是当前 `UGameInstance（游戏实例）`。它保存玩家资料、角色列表、选中角色、目标体验、世界分配摘要、恢复次数和 ObservationId（观察身份）等跨节点稳定值。

禁止在该上下文中跨地图强持有 `UWorld / AActor / APlayerController / UUserWidget`。世界对象必须从当前 GameInstance 重新取得并重新验证。

Endpoint（连接地址）和 TransferTicket（转移票据）只存在于 Private（私有）非反射字段；TransferTicket 交给 `GamePlatformSession（游戏平台会话）` 后立即清除，禁止写入 Blueprint（蓝图）、ViewState（界面状态）、日志或 Telemetry（遥测）。

## 3. 节点模型

正式节点身份包括：

```text
Boot
Initialize
Authentication
LoadProfile
LoadRoster
CharacterEntry
CreateCharacter
ValidateSelection
ResolveExperience
RequestWorld
TransferWorld
WorldReady
InWorld
Recovering
```

简单、一次性异步步骤复用 `UGamePlatformCallbackFlowNode（平台回调流程节点）`，避免为每个 HTTP 请求制造反射类型。需要等待真实外部事件的 `Authentication / CharacterEntry / WorldReady / InWorld` 通过平台层 `GamePlatformApplicationFlowNodes::CreateAwaitEventFlowNode（创建外部事件等待流程节点）` 直接复用现有 Callback Flow Node（回调流程节点）；该工厂不新增反射类型，创建的节点不保存项目 Payload、不创建 Tick，也不持有业务资源。项目协调器通过当前 `FGamePlatformFlowNodeToken（流程节点令牌）` 调用 `SubmitEvent（提交事件）` 精确推进。原项目层 `UDivineBeastsApplicationFlowNodeBase / UDivineBeastsPassiveFlowNode` 已删除，避免为纯机制重复建立项目继承层。

所有异步回调都必须先核对 ScopeId、RunId、NodeId 和 NodeGeneration。旧回调只进行常量级身份检查后直接返回，不能修改新流程数据。

## 4. 登录与角色流程

Authentication（认证）节点观察 `GamePlatformOnlineClient（游戏平台在线客户端）` 的真实认证状态。UI 只能发送 `TryAutoLogin / LoginWithCredentials` 命令，不能控制流程跳转。

认证成功后由平台流程进入 `LoadProfile → LoadRoster → CharacterEntry`。CharacterEntry 是等待节点：创建新角色或选择已有角色由 UI 提交语义命令，再通过具名 Outcome（结果分支）进入 `CreateCharacter` 或 `ValidateSelection`。角色所有权、版本、资格最终以后端校验为准。

## 5. 世界角色和体验

正式服务器角色只有：

```text
OpenWorld  # 常驻世界：登录大厅、主城、开放世界
Village    # 新手村：普通体验、Tutorial教学、Training训练
MainArena  # 短生命周期竞技实例：1v1～5v5
```

`Village.Tutorial / Village.Training` 是 Experience（体验），不是独立服务器角色。项目层通过 `FDivineBeastsProjectCatalog（神兽联盟项目目录）` 和 Definition（定义）解析目标体验，避免业务代码散落地图路径和服务器身份。

World Assignment（世界分配）始终由后端权威产生。客户端只能提交 CharacterId、ExpectedRevision、DesiredExperienceId、PreferredRegion 和 RequestId，不能自行选择服务器、生成 Endpoint/Ticket 或伪造 Admission（准入）。

## 6. Session 与 WorldReady

`TransferWorld（世界切换）` 把已授权 Assignment 转换为 `FGamePlatformSessionTransferRequest（平台会话转移请求）`，调用真实 `Session.BeginTransfer`。成功仅表示 Session 接纳操作，不代表网络已连接或服务器已准入。

`WorldReady（世界就绪）` 必须同时满足：

```text
SessionAdmission
ExpectedWorld
ExpectedExperience
CharacterBinding
GameplayData
ProjectReadiness
UDivineBeastsWorldDefinition 数据租约有效
Loading.IsReadyToPlay == true
```

NetworkConnected（网络已连接）和 AdmissionConfirmed（准入已确认）只能由 Session Transport（会话传输适配器）提供，项目层的本地事实接口不能伪造这两个事实。World/Controller 等本地事实使用 ObservationId 拒绝旧 Travel（世界转移）回调。

## 7. InWorld 与竞技边界

`InWorld（世界内）` 是长期异步等待节点，不是流程终点。正常游戏期间没有业务 Tick，只等待世界切换、Arena 分配、Session 断线、Logout 或 Shutdown 等事件，再通过 `SubmitEvent` 返回具名 Outcome。

Party（组队）、Matchmaking（匹配）、HeroSelection（竞技选人）、ReadyCheck（准备确认）、Match（比赛）和 Result（结果）属于 `GamePlatformArena / DBAArena`，不得塞入主 ApplicationFlow。MatchFound（匹配成功）得到 MainArena 世界分配后，才把“进入已授权世界实例”交回 ApplicationFlow。赛后必须重新申请新的 OpenWorld Assignment，不复用赛前 Endpoint 或 TransferTicket。

## 8. 恢复策略

`Recovering（恢复）` 清理当前 Loading 和 Session Transfer，旧 Ticket 立即失效，再重新请求 World Assignment。默认最大恢复次数为 3；超过预算返回安全状态。任何恢复都不得盲用旧服务器地址或旧 Admission 结果。

## 9. 性能约束

本模块明确禁止业务级 `Tick / FTSTicker`。稳定 GameInstance 服务只在 Initialize（初始化）阶段取得并缓存；流程运行期间不逐帧 `GetSubsystem`、不重复加载 FlowDefinition、不重复注册 NodeFactory、不逐帧刷新 ViewState。

平台调度已经改为一次性按需 Ticker：Start/节点切换只安排必要的即时 Pump，Completion/SubmitEvent 到达立即唤醒，Timeout（超时）与 Retry Delay（重试退避）按单调时钟精确唤醒；资产流程长期等待时仅以最多 2Hz 低频复核根 Data Lease。`OnSnapshotChanged（快照变化事件）` 只在公开快照真实变化时广播；UI 通过 `Snapshot → ViewState → ViewModel → Widget` 事件链更新，不轮询。

热路径避免无意义 UObject、临时 TArray/TMap、FString 格式化和高频日志。异步线程只准备值结果，UObject/World/UI 操作统一回到游戏线程。互斥锁只保护单槽完成邮箱，不在锁内执行 HTTP、JSON、UObject 或广播操作。

## 10. 当前验证证据

2026-09-27 当前 Runner 实际执行：

- `Tests/Architecture/ValidateProjectHeaders.ps1`：293 处自有头文件引用，0 缺失。
- `DivineBeastsApplicationFlowClient`：使用 UE5.8 `DivineBeastsArenaClient` 正式响应文件单模块编译成功。
- 本轮通用等待节点工厂与按需调度修改后，`GamePlatformApplicationFlowSubsystem.cpp`、`ApplicationFlowSubsystemTests.cpp`、`DivineBeastsApplicationFlowSubsystem.cpp` 使用 UE5.8 当前客户端目标响应文件定向编译成功。
- `GamePlatformApplicationFlow` 原生生产调度核心：加入 `NextWakeTimeContract（下一唤醒时间契约）` 后，Debug `Cases=32 Failed=0`；Release `Cases=32 Failed=0`。
- 全量 `DivineBeastsArenaClient`：尚未通过；本轮在 `-NoUBA -MaxParallelActions=1` 下重新执行 UBT/UHT，最新阻断发生在本任务范围外的 `GamePlatformAbilitySystem/Types/GamePlatformAbilityGrant.h`，UHT 无法找到 `UGamePlatformGameplayAbility` 与 `UGamePlatformAttributeSet`。由于 UHT 在此提前终止，不能据此判断后续模块状态，也不能把定向模块通过描述为完整 Client、Cook 或端到端联调通过。

后续完整验收仍包括 Editor/Client/Server 正式目标、UE Automation、PIE 多 GameInstance、OpenWorld/Village/MainArena 实例切换、真实 Session Transport 和 Cook/Stage。
