# 竞技入口服务器资格适配修复

用户报告新版新手村竞技门点击和 F 无反应。当前客户端日志已记录同一门的 `Requested=1`；在真实运行客户端读取 LastResult，服务器返回 `InteractorNotActive`，没有创建 Session。前轮点击与远镜头发现修复已存在，不能把请求已发送当作准入成功。

根因：GamePlatformInteraction 通过 IGamePlatformGameplayEligibilityProvider 读取资格。平台 Experience 世界的唯一 Active 权威在 GameMode / PlayerState 生命周期中；平台 PlayerController 尚未实现该接口，因此查询回退到角色上给非 Experience 宿主使用的中立 Eligibility 组件，该组件默认失活。移动和施法读取的现有生命周期已经 Active，两个读取入口没有适配到同一权威。

实施顺序：

1. 在现有离线 HostLifecycle 原生回归中，使用真实准入、准备、撤销和竞技门，验证准备前拒绝、Active 后成功提交、撤销后拒绝；先保留失败证据。
2. 由 GamePlatformPlayerControllerBase 实现既有资格接口，只读当前 GameMode 的 IsPlayerGameplayActive 与 PlayerState 的当前 PawnGeneration。不增加复制字段、状态机或可由客户端写入的 Active。世界退出、无当前 Pawn、代次越界均失败关闭。项目 WorldPlayerController 进一步复用角色已有 ASC EvaluateActivationEligibility，保持 Ready、角色代次、死亡与当前权威检查；死亡后的五秒重生等待不能仅因平台记录仍 Active 而开启门交互。现有非平台宿主的 Pawn 资格路径不变。
3. 执行受影响原生门禁，固定既定参数增量构建完整 Editor、Client、Server；对相同输入 Cook / Stage 双端，核对保护清单中的当前 216 份法术资源。
4. 受控替换实际 Village / MainArena，核对真实 PID、包路径、启动身份与共享战斗版本后启动两个客户端。分别验证真实门交互、竞技面板和正常匹配准入，不强制旅行或伪造匹配。

证据位于 Saved/Validation/ArenaEntranceRuntime20261101。保留主分支现有未提交修改，不提交或清理缓存；共享编辑器的地图、未保存状态和镜头已单独记录。用户已在两个相关聊天直接授权临时关闭 PID 65604 并恢复，执行前原生再次核对无脏包后正常退出。

RED：完整 Editor 14eb661a-d261-47bc-807f-1de0caf39a56 通过；原生 HostLifecycle a7ea5249-e0e5-476b-bc13-9d55be786520 经真实离线准入和当前准备令牌转为 Active 后，竞技门仍以错误终态拒绝，且资格接口为空，符合实机故障。前两次夹具修正（Controller 隐藏位置 API、离线基类需要显式组合交互组件）及 DLL 锁定失败均保留原始证据，不计为有效 GREEN。

代码复审确认必须保留死亡/Ready门禁，已通过项目派生覆写连接现有 GAS 查询。资格修复已通过完整 Editor c54fcebc、Client/Server 654df3e8 及 Client Cook ab6cbb18、Server Cook a77680e4；原生报告按首次 31 项、带正确 Hero 参数的 Warmup 补验 1 项和真实 HostLifecycle 1 项分别留证，未覆盖原始夹具失败。216 份当前法术资源指纹不变。实际 Village / MainArena 包和进程已更新。

## 竞技场转场握手修复（2026-11-02）

实机两个客户端均已通过门交互打开匹配界面，匹配并加载 L_DBA_MainArena；服务器也成功读取对应 Assignment。但客户端等待服务器确认 60 秒后超时，说明门资格问题已解决，真实竞技场准入仍未通过。

根因在现有 GamePlatformUESessionTransport：ClientTravel 后的 50ms 握手检查仍可能取得离开中的 Village World、旧 PlayerController 和旧 AdmissionHandshakeComponent，从而提前消费竞技场证明。修复记录本次离开的 World / Controller，只有当前有效的新 World、本地新 Controller 及其已注册且归属正确的握手组件才允许发布连接事实和提交证明；原有服务器确认绑定、去重、取消及权限校验不变，不另建 RPC、播放器或资格状态。

新增离线回归使用真实 Transport、本地 Controller 和握手组件，通过仅拦截旅行边界保留旧 World 来触发竞态。RED 2194b8f4 验证期望零次目标连接事实却收到一次；修复后完整 Editor 0b1078f8、完整 Client d363e2f3 通过，原生 50ebd54d 的全部 4 项会话用例通过。测试不发送真实凭据。日志新增 DestinationSelected 与 Confirmed 两个不含密钥的边界事实，用于后续联机定位。

仅 GamePlatformSession 私有实现和测试源码变化；其模块为 ClientOnly、仅允许 Client / Editor。两份完整 Editor 输入清单中另有四个引擎生成 FileOpenOrder 日志变化，其余源码、配置、协议和资产相同。85 个 Editor DLL 中 5 个发生变化，另外四个源码未变模块的真实重编、重链记录保留在夹具构建 1a3641ee；不能将此写为仅一个 DLL 变化。GamePlatformCombat、GamePlatformCore、GamePlatformAbilitySystem 的 DLL 指纹相同。继续使用已经同步构建及 Cook 的服务器包前，另行核对真实 Village / MainArena 当前 PID、启动身份、包指纹、UDP 和复制错误日志，不沿用已退出的实例记录。

本轮准入证据归 Saved/Validation/ArenaEntranceRuntime20261101/Admission。用户明确授权正常关闭 PID 59892 并恢复原新手村地图和镜头，关闭前原生确认没有脏包。客户端 Cook / Stage fbcd3ee8 已通过既有容器和颜色门禁，实际 Village / MainArena 的版本门禁通过，两个新版客户端 51804 / 49928 已启动，日志确认 t.MaxFPS=0、r.VSync=0。

恢复编辑器 PID 49836 的原新手村地图和镜头后，通过直接 Monolith 9316 再次读取确认：地图、位置、旋转匹配，dirty maps/content 均空，MaxFPS=0、VSync=0。首次恢复脚本尝试设置未暴露的 Monolith ServerPort 属性失败，未写配置；后改用实际已监听的 9316 完成恢复与独立检查，未把请求的 9317 当作真实端口。

版本检查首次因 HEAD 前进而停止；已读取新增提交与完整输入差异，确认提交仅收录已编译修复、文档与小地图制作脚本，没有新增原始契约或原生资产变化。重检同时覆盖历史及当前 Shared/Contracts、原生资产变化，保留首次阻断记录。兼容性门禁与运行进程检查见 FrontEndVersionGate.json 和 ClientOnlyCompatibility.json，不以 Git 提交号相同代替实际输入与进程核对。

当前 Windows 锁屏，已请求用户手动解锁、登录两个测试账号，未自动操作认证界面。真实新包 MainArena 准入确认、加载界面消失与玩法核验仍待完成，不能宣称竞技场准入已通过；匹配后还须将真实 Assignment 的 ServerId / BootId / 端口对应到核验实例。待验收快照为 Delivery.PendingManual.json。构建阶段已结束，独立 Stage 包保持不变；已向小地图任务交还 15 分钟原生 UI 制作窗口，要求冻结共享 C++、配置、契约与运行进程，并恢复原地图/镜头后交还。

## 确认后的项目世界就绪审查与修复（2026-11-02）

用户解锁并进入新手村后，真实 1v1 匹配为 step-83609e0b1c9ad4c60143df3a67915120ff97608c，目标实例 mainarena-local-99737be6089c4bd9a4843562ca5ad27d-0-3，Boot b4565390、UDP 17881，与当前 PID 52216 和已验证包一致。两个客户端分别在 UTC 12:32:08 / 12:32:10 收到 MainArena 的服务器确认，随后立即显示 ErrorReconnect，最终会话超时。因此前轮旧世界握手修复已在真实目标连接上生效，完整入场仍失败；前轮原生回归没有覆盖确认后的项目 Loading 整体就绪链。

第二处错误位于 DivineBeastsApplicationFlowSubsystem::NotifyWorldObserved。BeginLoadingForAssignment 只注册项目自定义就绪任务和定义加载任务，未配置内置 WorldPresence，TargetWorldPackage 为空。项目在已有 WorldDefinition 包身份、真实 GameInstance / World / BeginPlay 检查之后，又调用平台 ReportWorldOperable；平台按其内置目标包规则检查，真实地图与空包名不等，返回 WorldNotOperable。项目误投影 WorldMismatch 并提前返回，错过本次本地就绪事实报告。界面本地化未覆盖该错误码，因而只显示“操作未完成，请稍后重试”。本次未放宽平台规则，也未通过延长超时掩盖错误。

新增原生 WorldObservation.CustomReadiness 使用真实独立 GameInstance、基础 GameMode、BeginPlay、Loading 服务及正式项目任务。RED 7377c90d 通过所有世界前置条件，准确复现四个断言错误：早到通知错误写入 World 事实、等待定义时误报错误、有效世界观察被拒绝、有效观察后仍有 WorldMismatch。修复在定义未核实期间只等待；定义就绪后改用项目既有 IsWorldOperable，再报告本地事实。世界包、实例归属、生命周期和服务器准入均继续验证。

复审还发现 ReportLocalFact 可以同步发布 Ready，回调会释放项目 LoadingContext。原调用返回后继续解引用成员，存在失效访问风险。修复捕获操作 ID、共享 Context 与 Session 身份，并在第一条报告返回后复核代次和存活；同一回归增加真实 Session 同步 Ready 清理场景。该子场景使用明确的离线 Transport 事实，仅验生命周期，不能作为真实认证或网络验收。

用户明确允许正常关闭 PID 49836 并恢复，并要求直接完成、不要重复问询。关闭前已再次确认无脏包，记录地图镜头；216 份法术资产和已保存的 6 份小地图资产保持保护。小地图修改使原 UI 颜色报告失效，需真实 RHI 重新生成颜色证据，不能跳过发布门禁。本轮完整目标、原生 GREEN、颜色、Cook、版本门禁与真实双客户端入场证据归 Saved/Validation/ArenaEntranceRuntime20261101/WorldReady，逐项记录结果，不以准入 Confirmed 代替最终 InWorld。
