# 平台交互完成通知跨网补齐记录（2026-10-31）

本记录只覆盖`GamePlatformInteraction`的成功事实传输与客户端消费保护。平台代码不包含项目/竞技专属符号，不修改服务器交互判定、后端、资产、构建配置或运行进程。

## 根因与接口

原`FinishSession -> PublishEvent -> OnInteractionEvent`只在服务器本地执行，拥有者客户端的`OnRep_LastResult`只发布`OnResultChanged`。普通动态委托不会跨网，瞬时Active/Completed属性变化也可能被复制合并；因此客户端只监听`OnInteractionEvent`时收不到已确认成功。

组件增加受保护的`ClientReceiveCommittedInteraction(Event, Session, RecipientController, RecipientPawn)`可靠Client RPC。`PublishEvent`继续构造唯一Event，复用该值及不可变Session快照排队消息，再保留原服务器本地广播。`FinishSession`在结果监听前捕获原Controller/Pawn，结果回调重入后的拥有关系若不再匹配则不向新拥有者转投。Listen/Standalone本地权威不发送自身回声，客户端接收边界也拒绝权威宿主。

消费者仍只监听`OnInteractionEvent`。公开Request/Session/Result结构未更改。失败/取消和持续视图沿用既有OwnerOnly属性；新增通道仅传输`InteractionCommitted/PickupConsumed/HarvestCompleted`成功事实，不重执行Commit，不决定外部业务权威是否完成后续命令。

## 生命周期与拒绝合同

- `BeginFocusedInteraction`在发送Begin RPC之前记录不可变请求身份：目标弱引用、组件实例ID、代次、起始修订和Option。接收时同时校验Event、Session、内层Result的Request/Session/Option身份和成功终态，不从鼠标或当前会话猜目标。
- 目标、Interactor、原Controller/Pawn必须有效且属于同一当前World；目标销毁、错World、错接收者、未发送请求、错配快照和重复Session均不通知。
- 本地请求和去重历史各最多64条。回调前标记消费与Session去重，广播后不再访问组件状态；监听者同步重入、失活或清理不能再次消费同一消息。
- Pawn Controller变化、Controller Pawn变化、`SetLocalFocusPolicy`启停、World变化与EndPlay撤销旧作用域。失活后重显或恢复原拥有者只接受新请求；EndPlay先撤销接收资格再进入可重入的取消清理。
- 只在既有生命周期事件、发送命令和RPC接收边界维护记录，没有新增Tick、Ticker、定时器或业务轮询。记录不拥有Actor，也不保存业务权威状态。

## 修改文件

- `Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Public/Components/GamePlatformInteractorComponent.h`：RPC声明、客户端弱请求记录与作用域，补中文边界说明。
- 同模块`Private/Components/GamePlatformInteractorComponent.cpp`：发送前记录、不可变成功通知、完整接收校验、先去重后广播和生命周期撤销。
- 同模块`Private/Tests/InteractionClientCommitBridgeTests.cpp`：四个原生回归，直接驱动真实RPC接收实现、实际请求记录和真实动态委托。
- 同模块`Private/Tests/InteractionReentrantCommitFixture.h`：仅测试夹具增加一次性事件重入回调。
- 插件`Docs/API.md`、`AuthorityAndNetworking.md`、`WorldLifecycle.md`、`TestingAndEvidence.md`：接口、调用方、所有权、网络、失活和验证边界同步说明。

新增测试文件和本记录须由主任务同步到总体目录规划，避免覆盖该文档已有并行修改。

## 原生回归与实际验证状态

先写原生回归，再修改机制；本分工明确不运行Editor构建或启动运行时，因此没有本轮RED/GREEN、Automation成功、联机或Cook证据。

| 测试名 | 所保护行为 | 本分工状态 |
| --- | --- | --- |
| `GamePlatform.Interaction.Network.ClientCommitSnapshot` | 原成功快照独立于当前会话，重复只投递一次，本地权威无RPC回声 | 已写，未编译/运行 |
| `GamePlatform.Interaction.Network.ClientCommitRejectsInvalidIdentity` | 外层与内层全部身份、接收者、目标销毁、错World拒绝；拒绝不消费原始合法请求 | 已写，未编译/运行 |
| `GamePlatform.Interaction.Network.ClientCommitRejectsStaleScope` | Pawn及Controller宿主拥有关系换代，失活重显与EndPlay后迟到消息拒绝，新请求仍可消费 | 已写，未编译/运行 |
| `GamePlatform.Interaction.Network.ClientCommitReentrantDelivery` | 动态监听同步重复/失活安全、有界历史淘汰、同Session跨Request去重 | 已写，未编译/运行 |

实际已执行：`Build/Validation/VerifyInteraction.ps1 -WorkspaceRoot <实际工作区根>`退出0，输出“GamePlatformInteraction 静态门禁通过”；`git -c safe.directory=<实际工作区根> diff --check -- Game/Plugins/GamePlatform/World/GamePlatformInteraction`退出0。Git提示两个历史文档按仓库策略下次接触时LF转CRLF，无空白差异错误。只读核对锁定本机UE5.8的Actor网络角色API、本地玩家识别、Pawn拥有关系通知及组件BeginPlay/EndPlay前提，不以API读取代替编译。

仍需主任务统一Editor目标增量构建、精确运行上述四项及既有`GamePlatform.Interaction.Lifecycle.TerminalListenerStartsDifferentTarget`，并依据真实输出修正编译/行为问题。新增反射RPC要求Client/Server同步构建；真实拥有连接上的可靠传输、延迟/断线/重连、目标流送、Client/Server Cook与人工界面验收分别补证。直接调用RPC实现的Automation不能证明网络封送或通道可靠性。

中文审核范围为本次新增API/私有状态、接收分支、重入顺序、测试与插件文档；未对其他历史源码做全量中文整改或验收声明。
