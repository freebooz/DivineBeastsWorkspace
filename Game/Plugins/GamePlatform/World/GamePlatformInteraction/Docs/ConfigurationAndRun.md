# ConfigurationAndRun（配置与运行）

`DefaultGamePlatformInteraction.ini（默认交互配置）`配置 FocusRefreshInterval=0.10s（焦点刷新间隔）、HoldValidationInterval=0.20s（长按服务器复验间隔）、MaxHoldDuration=30s（单次长按最大配置时长）、最大交互距离600cm、视点起点偏移250cm、独立 Begin/Cancel（开始/取消）限流窗口、最近 Request（请求）缓存数量与 TTL（存活时间）。

这些是安全默认/开发默认，不是经过 Profile（性能分析）后的固定最优参数。调整后必须重新验证 Focus（焦点）响应、服务器 Trace（射线）数量、RPC（远程调用）速率和 Hold（长按）取消延迟。Begin 与 Cancel 使用独立窗口；WorldTime（世界时间）在无缝切图/世界重建时如发生回退，会自动重置本地节流、服务器限流窗口和过期幂等缓存，避免旧时间戳长期阻塞请求。

正式 Actor 组合需要：玩家拥有 Actor 上的 InteractorComponent；可交互 Actor 上的 InteractableComponent；服务器 GameplayEligibility Provider。正式 Input/Character/Online 组合由主工程适配。

当前 `DivineBeastsArena.uproject`显式启用 GamePlatformGameplay 与 GamePlatformInteraction；主工程模块只为 Development 测试胶水依赖这两个插件。
