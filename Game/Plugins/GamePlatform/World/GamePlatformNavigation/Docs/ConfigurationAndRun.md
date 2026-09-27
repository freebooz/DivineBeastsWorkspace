# ConfigurationAndRun（配置与运行）

`DefaultGamePlatformNavigation.ini`当前安全默认：MaxProjectionExtent=5000cm、MaxPathDistance=100000cm、DefaultRequestTimeout=5s、MaxOutstandingAsyncRequests=256、Invoker Generation/Removal 上限10000/15000cm、HighCostAreaMultiplier=5。

这些是源码安全上限/开发默认，不是经过性能测试的生产最优值。地图自身 Runtime Generation、SupportedAgents、RecastNavMesh、NavMeshBounds 和“仅Invoker周围生成导航”等设置仍由真实 UE 地图/Project Settings 决定。

AgentProfile 必须注册到当前 World Navigation Service 后才能以自定义 ProfileId 查询；未指定 Profile 使用 UE 当前默认 Supported Agent。

当前 EngineAssociation=5.8，但精确 Build/patch 未锁定且 Runner 无 UE_ROOT，不能提供 Editor/Client/Server 启动命令成功证据。
