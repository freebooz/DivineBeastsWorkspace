# NavigationBoundary（导航边界）

`GamePlatformNavigation（平台导航插件）`已进入P0源码实现。依赖方向为 `GamePlatformAIServer → GamePlatformNavigationServer`；AI服务器模块已删除对 `NavigationSystem` 的直接Build依赖，Controller也不再直接调用 `UNavigationSystemV1`。

Patrol 的随机可达点通过当前World的 `UGamePlatformNavigationWorldSubsystem`调用 `FindRandomReachablePoint`，并使用AIDefinition的 `NavigationProfileId`。Chase/ReturnHome/Investigate仍更新Blackboard GoalLocation，由BehaviorTree原生 `Move To`/UE PathFollowing消费，避免重复构造移动系统。

Navigation插件负责高级Query、Profile、Filter、Async/Cancel等统一边界；BehaviorTree原生Move To继续复用UE Navigation。AI不维护平行NavMesh、A*或Path Registry。

真实 NavMeshBounds、Dedicated Server NavData、MoveTo成功/失败、Partial Path、动态Modifier、Invoker等证据仍因测试地图与UE5.8工具链缺失而未执行。
