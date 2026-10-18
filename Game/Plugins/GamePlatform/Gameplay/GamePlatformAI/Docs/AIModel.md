# AIModel（AI模型）

每个 AI Pawn 使用 `UGamePlatformAIStateComponent`保存 AIEntityId、AIDefinitionId、PublicState、CurrentTargetEntityId、Movement/Combat Intent Tag、StateRevision 和 AIInstanceGeneration；持续状态复制给客户端，内部候选/Blackboard/路径不复制。

服务器 WorldSubsystem 只在非客户端世界工作：一次性处理世界已存在 Actor，并监听后续 Actor Spawn；带 AIStateComponent 且尚无 Controller 的 Pawn 自动生成 ServerOnly `AGamePlatformAIController`并 Possess。

`UGamePlatformAITargetComponent`提供 TargetEntityId、TargetGeneration、Enabled 状态，项目/MOBA以后可通过 `IGamePlatformAITargetEligibilityProvider`扩展安全区、队伍或任务关系，而不是恢复 Faction/FiveCamp/Element。

AIInstanceGeneration 在 UnPossess/复用边界推进；异步 Definition/Brain 资产回调必须匹配当前 Generation 才继续。

`BrainType=None`是已实现的原生轻量决策模式：定义租约加载成功后立即执行首次决策，随后复用定义中的决策间隔。无目标时通过平台导航服务选取出生锚点周围的可达点，由UE原生PathFollowing移动；没有另建项目巡逻计时器、每帧玩家扫描或客户端AI。

空闲移动以现有移动意图区分巡逻、归家和失效的追击。已提交的巡逻/归家路径延续到完成；巡逻完成后仍在巡逻范围内就选择下一点，不再将短归家容差误用为巡逻范围。距离使用水平厘米；地形高度和胶囊中心差不触发归家。目标丢失后停止旧追击路径，远离出生锚点时归家，接近时恢复巡逻；死亡/控制/失去占有沿用既有停机与清理入口。

首次无导航、可达点查询失败和原生MoveTo失败统一按1秒退避重试；不会发布虚假的Moving。原生AlreadyAtGoal发布Idle，巡逻间等待保留巡逻意图，下一次选择仍属于同一巡逻范围。重新占有与权威重生清除旧巡逻退避；零巡逻半径明确停用游走，但不关闭脱战归家。

开发专服追加`-LogCmds="LogGamePlatformAI Verbose"`可将既有移动请求与真实状态/意图变化写入文本日志。`AIMovementRequest`包含Pawn、AIEntityId、代次、巡逻/归家、MoveTo结果、目标实体、当下位置、目标点、出生锚点及LastError；NavigationError保留平台导航查询的具体错误，区分无NavData、未注册过滤器与无路径，避免统一AI错误掩盖根因。`AIState`包含最终公开状态/意图及位置。没有新增Tick、计时器或逐帧日志。验收必须按同一AIEntityId与代次比较后续位置，确认无目标巡逻真实位移及再次选点；单条RequestSuccessful仅表示移动请求受理，不能证明路径完成或角色移动。无可达点时Goal为Unavailable，实际导航查询失败与MoveTo失败通过Result区分。

2026-10-18验证：`AIWorldPolicyTests.cpp`新增`GamePlatform.AI.Movement.IdlePatrolLifecycle`，包含16个纯规则输入及真实控制器首次请求、无导航退避、MoveTo拒绝、死亡停机和重新占有调用。生产私有函数及同组断言在`Saved/Validation/MonsterPatrolPolicy/`独立MSVC抽取编译，旧条件失败、新条件16项通过；该证据只覆盖纯规则。UE Automation、真实Village NavMesh恢复、专服移动/客户端动画与联机验收由主流程另行执行，不能将此记录视为已通过。

2026-10-18后续实测：原生AI生命周期在Editor及真实Development Server上通过；最终Server测试报告 `Saved/Validation/FoundationM0/22298c1f-5339-4607-918b-1b03031ed463/NavigationServerRegression/result.json` 实际2项Success（含导航冷缓存）、0失败、进程退出0。最终Server构建 `7c17ea7e-d4b2-4244-a7c3-c95b914c3ea9` 与Cook `76fee423-823e-429c-bd37-f61ef64a063c` 通过。实际Village专服10条 `VillageWolfSpawned` 与10个AI实体/代次按出生XY逐一匹配；每只均有连续巡逻请求和真实水平位移，观测最大离初始位置为360.834—403.238cm，超过旧160cm误归家范围。偶发原生可达点查询失败通过既有退避恢复，未将请求受理当作移动证明。证据 `Saved/Validation/SkillBar20261018/PatrolAfterNavigationFix.json`；客户端动画和联机人工验收仍独立待确认。
