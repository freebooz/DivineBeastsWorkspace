# Architecture（架构）

`GamePlatformNavigation（游戏平台导航插件）`正式位于 `GameFoundation/Gameplay`，由 `GamePlatformNavigation（共享Runtime模块）`和 `GamePlatformNavigationServer（ServerOnly服务器执行模块）`组成。旧 `GameFoundation/World/GamePlatformNavigation` 仅为空骨架，本轮迁移同一插件身份，不建立平行导航框架。

共享模块只定义 AgentProfile（导航代理配置）、Request/Result/PathPoint、RequestHandle、错误、PartialPath/Invoker/Authority 策略和服务接口；不依赖 NavigationSystem、AIModule、AI、Combat、Interaction、UI 或 VFX。

服务器模块依赖 UE `NavigationSystem（导航系统）`、`AIModule（AI模块）`和 `GamePlatformWorld（平台世界插件）`，实现 UWorldScoped（世界作用域）导航服务、NavData/Filter选择、同步/异步路径、动态Modifier、SmartLink、Invoker和诊断。

AI依赖方向已迁移为 `GamePlatformAIServer → GamePlatformNavigationServer`；NavigationServer 不依赖 AI。原生 BehaviorTree `Move To`仍使用 UE PathFollowing，不重复封装。
