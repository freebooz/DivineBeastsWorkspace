# AIIntegration（AI集成）

上一轮 `GamePlatformAIServer（AI服务器模块）`的直接 `UNavigationSystemV1`依赖已删除，Build.cs 改为依赖 `GamePlatformNavigationServer（平台导航服务器模块）`。

`AGamePlatformAIController::RequestPatrolGoal`现在从当前 UWorld 获取 `UGamePlatformNavigationWorldSubsystem`并调用 `FindRandomReachablePoint`；AIDefinition 的 NavigationProfileId 直接传给导航服务。

BehaviorTree模式通过Blackboard GoalLocation与行为树原生`Move To`移动。`BrainType=None`的原生轻量模式已通过`AAIController::MoveToLocation/MoveToActor`执行Patrol、ReturnHome和Chase；高级可达点查询仍进入平台导航，移动执行始终复用UE PathFollowing，不重复造移动框架。NavigationProfileId为None时使用引擎默认Supported Agent，不代表导航配置丢失。

AI Death/Stun仍由AIController StopMovement/Brain策略处理。2026-10-18空闲移动修复保留正在执行的巡逻/归家路径，在目标丢失时取消旧追击，并核对原生MoveTo的失败/已到达结果；导航服务、可达点或移动受理失败后进入现有决策时钟的有限退避。具体测试边界见[AI模型](../../../Gameplay/GamePlatformAI/Docs/AIModel.md)；纯规则通过不代表真实NavMesh或专服MoveTo已经运行验收。
