# ArenaExtensionBoundary（竞技扩展边界）

核心 ApplicationFlowClient 不依赖 GamePlatformArenaClient、MobaPresentation 或 DivineBeastsArena。

IDivineBeastsApplicationFlowExtension（应用流程扩展接口）提供 EnteredInWorld/LeavingInWorld 通知，Arena组合模块可由主工程组合根注册并编排 Party、Matchmaking、MainArena transfer、Arena selection/ready 和 PostMatch。

Arena插件关闭时，登录→角色→OpenWorld/Village核心流程仍保持独立。PostMatch返回世界必须重新请求OpenWorld Assignment，不能缓存旧Endpoint。
