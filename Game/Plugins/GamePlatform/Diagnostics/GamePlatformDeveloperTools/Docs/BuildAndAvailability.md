# BuildAndAvailability（构建与可用性）

GamePlatformDeveloperTools.uplugin 将模块 Type 固定为 Editor，并限定 TargetAllowList=Editor，TargetConfigurationAllowList 不包含 Shipping。

Target.cs 双重门禁：
- DivineBeastsArenaClient.Target.cs：始终 DisablePlugins GamePlatformDeveloperTools。
- DivineBeastsArenaServer.Target.cs：始终 DisablePlugins GamePlatformDeveloperTools。
- DivineBeastsArenaEditor.Target.cs：Shipping 禁用，其他编辑器配置启用。

这比单独依赖 bBuildDeveloperTools 更严格。真实 Shipping 包仍需 ValidateCook/ValidateRelease 对构建工件进行检查。