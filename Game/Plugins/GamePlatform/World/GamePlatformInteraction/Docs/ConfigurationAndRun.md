# ConfigurationAndRun（配置与运行）

`DefaultGamePlatformInteraction.ini`配置 FocusRefreshInterval=0.10s、HoldValidationInterval=0.20s、最大交互距离600cm、视点起点偏移250cm、Begin/Cancel限流、最近Request缓存数量与TTL。

这些是安全默认/开发默认，不是经过 Profile（性能分析）的最佳参数。调整后必须重新验证 Focus 响应、服务器 Trace 数量、RPC 和 Hold 取消延迟。

正式 Actor 组合需要：玩家拥有 Actor 上的 InteractorComponent；可交互 Actor 上的 InteractableComponent；服务器 GameplayEligibility Provider。正式 Input/Character/Online 组合由主工程适配。

当前 `DivineBeastsArena.uproject`显式启用 GamePlatformGameplay 与 GamePlatformInteraction；主工程模块只为 Development 测试胶水依赖这两个插件。
