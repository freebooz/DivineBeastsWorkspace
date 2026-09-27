# AIIntegration（AI集成）

上一轮 `GamePlatformAIServer（AI服务器模块）`的直接 `UNavigationSystemV1`依赖已删除，Build.cs 改为依赖 `GamePlatformNavigationServer（平台导航服务器模块）`。

`AGamePlatformAIController::RequestPatrolGoal`现在从当前 UWorld 获取 `UGamePlatformNavigationWorldSubsystem`并调用 `FindRandomReachablePoint`；AIDefinition 的 NavigationProfileId 直接传给导航服务。

Chase/ReturnHome/Investigate 仍只更新 Blackboard GoalLocation；实际移动继续由 BehaviorTree 原生 `Move To`和 UE PathFollowing 完成。这样高级查询进入平台导航，移动执行不重复造一层 PathFollowing。

AI Death/Stun 仍由 AIController StopMovement/Brain策略处理；真实 Patrol/Chase/ReturnHome NavMesh 运行、无路径失败和专服 MoveTo 均未执行。
