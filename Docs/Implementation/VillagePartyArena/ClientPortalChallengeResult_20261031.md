# SSLM 竞技入口与固定对手约战客户端结果

日期：2026-10-31。范围为DBAArena的DivineBeastsArenaClient源码、原生回归及经主任务定点授权的DBAArena.uplugin两项依赖。本文不代替主任务统一构建、原生资产和联机结果。

## 源码实现

- `OpenMatchmakingMenu(PartyId, ModeId=NAME_None)`沿用唯一竞技页面；预选只接受现行模式目录身份。真实准备/队列/邀请/候选或写入结果未知时，第二个门不得改变已提交模式。
- 本地浏览需求与服务器状态分开。主动浏览在完整Idle通知后保持打开；关闭页面只撤页面需求，同一队列的后继通知不重开；新约战邀请及真实候选仍触发通知。关闭不会取消服务器票据、邀请或释放名单。
- 关闭完整面板后，真实准备/队列/邀请或未确认状态通过`UI.HUD.ArenaQueueStatus`普通非模态HUD继续展示，提供重开面板和同一取消命令入口；Idle、候选弹出、迁移、Controller/World清理时移除。它与场内HUD共用现有Data租约/加载器/平台AttachHUDWidget，不捕获CommonUI激活栈或创建第二传输/资源路径。
- 本地玩家按Controller/World生命周期订阅唯一Interactor的成功`InteractionCommitted`，核对本人Controller、目标入口类型、World、OptionId及RequestId/SessionId后读取`GetArenaModeId()`。Controller换代、EndPlay、World清理和子系统退出成对解绑；没有Tick扫描、碰触旅行或本地OpenLevel。
- 同一WBP增加快速匹配/好友约战页签、公开玩家ID输入与发起/接受/拒绝/取消。未接入好友服务，不展示伪造好友。多人约战客户端先检查本方满编/队长，后端仍为唯一名单和资格权威。
- 通过既有Online适配器提交`challenge`、`challengeReply`、`challengeCancel`。复用可信认证、clientRequestId幂等、本人观察及30秒候选确认/迁移/返回链，没有新传输服务、匹配器或旅行器。
- 邀请解码检查十进制字符串revision、双方名单互斥且完整、模式容量、本人可见性、回复/取消主体、截止时间及内部权限。取消/拒绝/超时的无ticket终态按真实合同收敛。
- 命令成功先单独撤销旧长观察代次，避免迟到观察把旧邀请复活，不取消刚被新状态触发的迁移。未知响应撤销`bKnown`按钮资格并完整重同步，不把超时当成未提交重复发起。认证换代和World换代过滤旧命令回调。
- Build.cs新增私有模块依赖`DBAWorldsRuntime`、`GamePlatformInteraction`；uplugin新增两项真实插件依赖，仅Client/Editor，保持原目标装配。

## 真实WBP控件合同

原有资产仍为同一`WBP_DBA_UI_Arena_Matchmaking`，C++父类仍为`UDivineBeastsMatchmakingScreen`。原有`ModeList`、`SelectedModeText`、`StartMatchmakingButton`、`CancelMatchmakingButton`、`SearchStatusText`、`ElapsedTimeText`、`CommandErrorText`、`CloseButton`等保留。

| 新控件名 | 类型 | 用途 |
| --- | --- | --- |
| QuickMatchTabButton | UButton | 快速匹配页签 |
| ChallengeTabButton | UButton | 好友约战页签 |
| ChallengePanel | UPanelWidget派生，如UVerticalBox | 容纳约战表单；页签切换可见性 |
| QuickMatchPanel | 可选UPanelWidget | 容纳原快速匹配控件；未创建也会单独隐藏原开始/取消按钮 |
| TargetPlayerIdInput | UEditableTextBox | 对方公开玩家ID手输 |
| SendChallengeButton | UButton | 提交固定对手邀请 |
| AcceptChallengeButton | UButton | 接收队长接受 |
| DeclineChallengeButton | UButton | 接收队长拒绝 |
| CancelChallengeButton | UButton | 发起主体取消 |
| ChallengeStatusText | UTextBlock | 邀请/不可用/截止提示 |
| ChallengeRosterText | UTextBlock | 服务器确认的甲乙双方名单 |

全部标签、输入、条目文字使用10号。新增原生状态文字及输入样式也设为10号。按钮标签可自行采用`<ButtonName>_Label`，原生绑定只依赖上表控件名。接收/取消按钮按当前本人权限显示，未知状态禁用；没有Widget计时器。真实WBP修改、布局、编译、保存和重载由主任务通过Monolith完成。

新增非模态HUD资产为`/DBAArena/UI/HUD/WBP_DBA_UI_ArenaQueueStatus`，父类`UDivineBeastsArenaQueueStatusWidget`（继承既有`UGamePlatformMobaArenaHUDBase`）。普通HUD层挂载，不改WorldHUD或输入模式。控件合同：`QueueModeText`、`QueueStatusText`和可选`CommandErrorText`为UTextBlock；`OpenMatchmakingButton`、`CancelMatchmakingButton`为UButton，后者的可选`CancelMatchmakingButton_Label`动态显示取消约战/退出匹配。全部字号10，建议面板占角落且不覆盖世界操作。发起方可取消约战，接收方通过重开进入接受/拒绝；未知状态取消禁用。真实资产由主任务制作。

## 回归与证据

先增加真实邀请信封成功断言再修改生产解码器。主任务已实际观察RED：`DivineBeasts.Arena.Backend.CommandEnvelope`失败于“指定双方邀请通过唯一解码器”，此前Editor增量UBT 10 actions成功，run `ac2a8d2c-540b-465e-87ec-265a67e7dd20`。证据为`Saved/Validation/VillageArenaPortal20261031/Red/Automation/index.json`。这一证据只证明基线缺能力的原生失败，不代表最终GREEN。

最终待主任务统一运行的相关回归：

- `DivineBeasts.Arena.Backend.CommandEnvelope`：真实信封、请求身份、邀请投影、双方名单重叠、缺邀请、过期回复权限、非本人、无票据取消终态、认证换代清理。
- `DivineBeasts.Arena.UI.MatchmakingCommandGate`：未认证/未有真实邀请时，新增约战操作不会调用远端副作用。
- `DivineBeasts.Arena.UI.PortalModePreselection`：合法门预选、非法身份拒绝、未知报名锁定模式、确认Idle后释放选择。
- `DivineBeasts.Arena.UI.MatchmakingMenuDemand`：Idle保持浏览、关闭保留非模态队列HUD且不取消/不随同状态重开、候选/迁移/Idle切换隐藏队列HUD。
- `DivineBeasts.Arena.UI.ChallengePanelLifecycle`：真实原生命名控件页签接线、未认证禁用、失活解绑。
- 原有`DivineBeasts.Arena.UI.KeyboardModeSelection`、`ConfirmedOfferTerminal`及页面归属回归保留；SurfaceInventory同步为5个完整Screen和2个普通HUD，页面归属夹具在明确关闭后重新打开时显式建立新的浏览需求。

本分任务实际执行：`git diff --check`通过（Git行尾提示不算编译结果）；uplugin JSON可解析，两项依赖均读取为Client/Editor。自审已核对唯一Online/观察/旅行路径、无新增Widget轮询及旧观察回包竞态。没有启动任何编译、Monolith或客户端进程。

尚未执行：最终Editor/Client/Server构建、上述最终原生GREEN、实际WBP与五门资产、Cook/Stage、真实服务器版本门禁、双会话1v1及4/6/8/10会话多人准入、人工视觉/玩法验收。由主任务在中央执行记录补证；不得将本源码结果写为联机或人工通过。

## 用户追加：五门共享旋涡的运行协调

新增本模块私有`World/DivineBeastsArenaEntranceVFXWorldSubsystem.{h,cpp}`，只在Game/PIE客户端World、非专服、非Commandlet创建。世界初始Actor快照加Actor生成/EndPlay通知维护门账本；PrimaryLocalPlayer增删事件维护唯一项目表现拥有者。重复门观察不会新增绑定或请求，不使用Tick/计时器扫描。

只借用`UDivineBeastsPresentationClientSubsystem::ActivateContentPack`的现有事务完成Definition与VFXRuntime资源预载、Provider类型门禁及目录发布，Active才提交平台Presentation请求。五门共用一个世界内容事务和单一Definition；每个门独立中立RequestId。预载/注册失败不播放、不循环重试、不影响交互；门EndPlay、主玩家变化、撤包及World退出先取消自有请求，再撤销目录与自身租约。提交过程重入退出由原提交栈收尾取消；旧拥有者Active回调不能恢复请求。

运行合同：

- 真实NS：`/DBAWorldPack_Village/VFX/Activities/Arena/NS_VillageArenaGate_Vortex`，由主任务Monolith创建并保存。
- Definition：`UGamePlatformVFXAttachedDefinition`，逻辑身份`divinebeasts.vfx.village_arena_gate_vortex@1`，Attached行为、Persistent请求、Low/Ambient优先级、无参数覆盖、空schema。Definition应循环、`bAutoDestroy=false`、`MaxLifetimeSeconds=0`、`bRequireFixedBounds=true`。
- 项目原生tag：`Presentation.Environment.ArenaEntrance.Vortex`；内容事务/目录Owner为`DBA.Village.ArenaEntrance.Vortex`，ProviderChannel=`VFX`，World生命周期，只有该ContentPackId上下文匹配。
- 挂门RootComponent，世界位置为Actor局部`(0,0,210)`厘米转换结果，朝向为ActorRotation。已读取真实`GamePlatformVFXNiagaraExecutor`：附着使用`EAttachLocation::KeepWorldPosition`，因此此请求保留210厘米相对偏移，Definition不要重复加偏移。
- NS局部平面为YZ；由主任务确认Y半径不超过145厘米、Z半径不超过184厘米、X深度不超过12厘米；建议固定bounds为`(-35,-170,-215)`至`(35,170,215)`厘米。美术、局部空气折动、伸缩质量档及成本实测仍由原生资产/GPU审核确认，本代码不宣称实际视觉通过。
- 新增私有显式依赖`DivineBeastsPresentationClient`、`GamePlatformPresentationClient`、`GameplayTags`。前两模块分别属于已声明的DBAClient、GamePlatformPresentation插件；不新增Niagara/VFX模块依赖，不修改世界或服务器反向依赖。

先写新增原生回归源码再实现协调器，尚未由本分任务编译或观察新增RED/GREEN：`DivineBeasts.Arena.VFX.EntranceLifecycle`覆盖首次快照/重复观察、缺Primary/注册不可用、旧Active回调和退出后迟到事件；`EntranceCancellation`通过真实平台Submit边界核对取消原身份、同提供者及重复移除；`EntranceRegistrationFailure`实际调用未初始化的项目内容服务，验证申请失败且没有残留内容包/预载，重复通知不反复申请。该测试不伪造Data成功或Niagara播放成功。最终新增C++需主任务重新构建Editor/Client/Server并运行上述回归；此前15项GREEN不能覆盖本追加。
