# GamePlatformNavigation（游戏平台导航插件）

跨游戏通用导航契约与服务器权威 Navigation System（导航系统）桥接。正式路径为 `Game/Plugins/GamePlatform/World/GamePlatformNavigation`，由 `GamePlatformNavigation（共享Runtime模块）`和 `GamePlatformNavigationServer（ServerOnly服务器模块）`组成；本轮从旧 World 分类空骨架迁移同一插件身份。

已实现源码：AgentProfile、Request/Result/PathPoint/Handle、ProjectPoint、TestPath、同步/异步FindPath、Cancel/Timeout/WorldGeneration、PartialPath策略、Filter Registry、Default/HighCost/Blocked NavArea、NavModifier组件、SmartLink桥接、Navigation Invoker、Diagnostics，以及 Character Capsule/NavAgent一致性校验。

上一轮 `GamePlatformAIServer` 的随机可达点直连已迁移到 `UGamePlatformNavigationWorldSubsystem`；BehaviorTree原生 Move To/PathFollowing继续使用UE原生能力，不重复封装。

当前0个 `.uasset/.umap`，因此真实NavMeshBounds、RecastNavMesh、动态重建、SmartLink到达、Invoker Tile、World Partition、Dedicated Server NavData、压力、Build/Cook均待UE5.8工具链和真实测试地图后验证。

