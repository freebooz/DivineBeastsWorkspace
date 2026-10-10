# 玩法、世界与竞技整改报告（2026-10-09）

基线 `0f6afcb571185626487c4c5da3ee750a52b71bb0`，分支 `codex/gameplatform-audit-fixes-20261009`。原GW-01至GW-17逐项核当前源；本子任务无commit/push、UBT或资产/生产操作。追加独立复核整改已源码冻结，根负责全局目录/描述与统一引擎验证。

分配领域当前修改46个、新增21个文件（不含根维护.uplugin）。完整清单、当前行号及实际命令见RepairReport.json。

| 审查项 | 当前状态 | 实现证据 |
| --- | --- | --- |
| GW-01 | 整合基线已消除；真实GAS运行待UE验证 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Private/Abilities/GamePlatformGameplayAbility.cpp`:18 |
| GW-02 | 源码装配与真实死亡事实桥接已补；完整UE/地图/联机未验收 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime/Private/Characters/DivineBeastsCharacter.cpp`:47 |
| GW-03 | 本轮源码接线；待真实握手/后端联调 | `Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArenaServer/Private/Server/GamePlatformArenaServerSubsystem.cpp`:151 |
| GW-04 | 重连与死亡复活受理源码已补；真实到期Spawn/Possess待UE | `Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaGameplayLifecycleAdapter.cpp`:468 |
| GW-05 | 追加修复完整快照与按任务实例重放；UE行为回归未执行 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp`:213 |
| GW-06 | 整批幂等与广播重入源码修复；生产Quest端口缺交付 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Private/Subsystems/GamePlatformQuestServerSubsystem.cpp`:1299 |
| GW-07 | 在飞重复ID基线已修；追加完成后复用ID的旧取消句柄修复 | `Game/Plugins/GamePlatform/World/GamePlatformNavigation/Source/GamePlatformNavigationServer/Private/Subsystems/GamePlatformNavigationWorldSubsystem.cpp`:441 |
| GW-08 | 整合基线已消除；原生规则通过/真实129次PCG待验证 | `Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp`:349 |
| GW-09 | Custom及终态广播重入源码修复；UE行为回归未执行 | `Game/Plugins/GamePlatform/World/GamePlatformInteraction/Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp`:1313 |
| GW-10 | 缺交付条件；正确失败关闭保持 | `Game/Plugins/GamePlatform/World/GamePlatformWorld/Source/GamePlatformWorld/Private/Subsystems/GamePlatformWorldSubsystem.cpp`:47 |
| GW-11 | 部分接口复用；完整Experience迁移待设计/资产评审 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/Docs/Architecture.md`:9 |
| GW-12 | 目标隔离由主执行者统一；全量中文存量未审完 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformQuest/Source/GamePlatformQuestServer/Public/Interfaces/GamePlatformQuestPersistencePort.h`:21 |
| GW-13 | 授权保留的未实现能力；不计功能完成 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/Source/GamePlatformAnimation/Private/GamePlatformAnimation.cpp`:3 |
| GW-14 | 本轮文档修复 | `Game/Plugins/GamePlatform/Gameplay/GamePlatformGameplay/README.md`:5 |
| GW-15 | 整合基线已消除；双World实机待验证 | `Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaServer/Private/Server/DivineBeastsArenaServerProjectExtension.cpp`:50 |
| GW-16 | 缺批准配置/地图；正确NotConfigured保持 | `Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaRuntime/Private/Catalog/DivineBeastsArenaModeCatalog.cpp`:22 |
| GW-17 | 本轮源码修复；Travel/迟到复制待UE验证 | `Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Client/DivineBeastsArenaUIClientSubsystem.cpp`:64 |

## 独立复核追加整改

- Quest重放保存(QuestId,QuestInstanceId,EventId)，Q1已提交时只重放Q2；同版本完整快照校验共同用于Load/Reconcile/Persist，坏响应保留旧Quests/载荷/归属。
- Quest同步广播后按PlayerRuntimeId重新Find，注销/新注册/Deinitialize不再访问旧引用。
- Navigation句柄追加正OperationGeneration；旧两字段及旧A句柄取消失败关闭，B记录仍有效。
- Interaction终态保存A不可变快照，OnResultChanged中开另一目标B不混淆事件或覆盖B会话视图。
- 项目Pawn中立服务器死亡委托由竞技私有Adapter消费，核World/Match/Roster/当前受控Pawn与State/代次，先解绑、同终态一次。RelatedPlayerId空，不猜击杀或胜利。复活true仅Timer受理，零秒NextTick，失败保持Inactive并明确错误。

## 实际验证与限制

首轮现有7个Native入口Debug/Release共35条配置/构建/测试命令、22次测试执行均0。追加只重跑Quest/Navigation，8条构建/测试命令、4次测试执行均0；验收矩阵共43项成功Native命令、26次测试执行；终审又复跑本轮8项命令/4次测试，实际累计51次成功Native命令执行、30次测试执行（重复验证不增加不同用例数量）。追加编译红灯2次退出1（新合同尚未实现），不是UE行为红灯。分配目录diff空白检查退出0。全部日志在本目录；一次单行编排失败后保存脚本重跑，失败没有记作通过。

本领域仍未执行；新增/扩展Private/Tests已源码冻结，供根统一锁定UE5.8构建/运行，不声明Automation通过。

Private/Tests新增/扩展覆盖：两任务共享事件首成功次冲突；同Version缺目标/错目标/无效或变更实例；持久损坏成功回包；广播注销/Deinitialize；A完成同ID启动B旧取消拒绝；交互终态开始B；真实Combat死亡事实一次及竞技桥接Timer受理/旧Pawn拒绝。所有UE回归尚未运行。没有宣称真实死亡到期成功复活、完整Hero预热出生或联机通过。

五模式继续NotConfigured；GW-10正式世界会话前提、GW-11完整Experience继承迁移及真实Blueprint父类、GW-13交互额外权限Provider、GW-16批准生产地图与胜利配置仍是明示交付条件。Quest真实生产端点/事务/Outbox缺失，击杀/助攻/目标可信桥接及服务器Cook/网络仍待交付。

## 兼容与人工中文审核

- Navigation句柄追加BlueprintReadOnly int64 OperationGeneration；0/旧手造两字段句柄失效，必须保存FindPathAsync原返回值，不提供按裸ID取消降级。
- ADivineBeastsCharacter新增中立FDivineBeastsAuthoritativeDeath/OnAuthoritativeDeath；有DBAArena实际消费者，未反向依赖Arena。客户端不发布，终态按AvatarGeneration一次，消费者拥有解绑。
- Quest快照未新增/重命名协议字段，持久端须返回完整同实例权威值；Completed历史保留当前Definition缺失/变更版本兼容，但不猜历史目标，不接受无效实例/修订/完成身份。

人工核读新增句柄/API兼容、Quest按实例所有权与完整快照/历史兼容、广播重入、死亡事实可信来源、终态一次、先解绑与零秒异步受理/失败语义、测试替身边界；同步6份受影响局部中文文档。不宣称存量公开字段全量完成。

新增目录增量为QuestSnapshotReentryFixture.h、NavigationReusedHandleTests.cpp、DivineBeastsArenaDeathBridgeTests.cpp（均在实际模块Private/Tests）。根负责登记全局目录规划；没有新模块/插件身份或反向依赖。

后续必须按锁定UE5.8实际Editor/Client/Server构建与Automation、真实资源/地图/联机/Cook/中文人工复核补证。本报告中Native和源码证据均不代替这些结果。

## 第二轮：死亡统计与结束后的复活重入

GameMode广播前捕获原World、Match/Server、BindingId、PlayerState、Pawn/Controller与代次，统计广播/失活/复活受理返回后重新核对；EndMatch或World退出撤销后续动作。Adapter复活Timer保存原RequestId及完整拥有上下文，先核请求身份，再清理自身并核InProgress和原Match/连接/Pawn代次。旧回调不能清新Timer；Spawn自身也在出生点/Restart/初始化/预热/激活外部边界重新核原作用域。

新增纯虚只读CapturePlayerGameplayOwnership与FOwnership值，所有既有实现/测试端口同步。它不拥有/授权Pawn，也不代表Ready/Active；死亡重连尚无Pawn可保留原正代次。GameMode绑定替换签发BindingId，提供只读身份与权威阶段核验API。无新增文件、模块依赖或反射/协议身份迁移。

既有DeathBridge用例扩为NormalAndOldRequest、StatsListenerEndsMatch、QueuedRespawnAfterEnd三个命令：真实Combat死亡统计监听者EndMatch后不得再排复活；已排请求结束后显式投递迟到回调必须拒绝并保持Pawn/代次；旧RequestId不得清新Timer。UE行为尚未执行。第二轮仅只读合同检查（红1/绿0）与领域diff检查0；没有Arena既有Native入口，不运行无关Native或自行UBT。Server源码已向根发出完整冻结信号，等待统一锁定Server构建。

中文人工审核覆盖Capture参数/失败与空Pawn兼容、绑定代次、广播/命令重入、请求/Timer所有权、出生边界、取消/失败及测试端口限制；同步DBAArena与Arena局部中文说明。根已修Quest Flush初始查找和Admission独立Stop/Reset门闩保持。

第二轮同批补齐旧Adapter资格所有权：清理自身Timer/订阅不要求仍为当前Binding；资格写入必须当前Binding、自有Pawn、当前Controller/Pawn.PlayerState/State.Owner、可信Roster及Combat/Gameplay正代次匹配，广播后再次核验。既有正常用例追加新Adapter/Pawn/Timer存在时旧失活拒绝、旧自有Timer已清、新资格/Timer保留与旧析构安全。没有新增文件或API，UE行为仍未运行。

## 首轮锁定Server构建失败后的编译修正

根执行者实际锁定UE5.8.0构建DivineBeastsArenaServer Win64 Development首轮失败，UBT退出6。完整日志为`Saved/Validation/FoundationM0/d7a0e3ef-993d-47c3-a033-019a9b6e47bc/Build/Server/UBT.log`；本域读取全量40条诊断，其中本域33条含连锁诊断，其他组Equipment/Telemetry共7条已通知根，没有修改其文件。完整实际命令及诊断见RepairReport.json与ServerCompileOriginalDiagnostics.json。

- ClearTimer(FTimerHandle&)要求可变句柄；从只读自有记录复制OwnedTimer/OwnedDeadlineTimer后清理，不改变请求身份、取消顺序或所有权。其余本域ClearTimer调用已核为可变自有记录/成员。
- UE5.8 Controller.GetPawn三元表达式返回TObjectPtr，明确APawn*接收，保留空值/拥有关系分支。
- DeathBridge测试明确TSharedPtr持有真实Adapter生命周期，使Get()为指针并支持Reset()；弃权使用已定义PlayerForfeit枚举。未删除或关闭测试。
- 角色Momentum初始化局部ASC重命名MomentumAbilitySystem，消除C4456遮蔽错误，不降低警告等级或改变初始化行为。
- 资格组件补GameFramework/Actor.h、竞技Controller补Engine/World.h，以完整Owner/World类型调用现有API；同步中文文件职责与句柄约束。

本次只修7个既有CPP文件，无新API/字段/模块依赖/新文件。PlayerController.cpp新增纳入修改清单。领域diff检查退出0（ServerCompileFixDiffCheck.log）；未删除测试、降告警或重跑无关Native。本子任务未自行UBT或运行Automation；源码重新冻结给根增量重跑，不能把这些编译修正当Server构建或死亡/复活行为通过。冻结哈希见ServerCompileFixSourceFreeze.json。
