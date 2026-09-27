# BuildAndAvailability（构建与可用性）

GamePlatformDebug.uplugin（调试插件描述）对两个模块设置 TargetConfigurationAllowList（目标配置允许列表）：Debug、DebugGame、Development、Test（调试/调试游戏/开发/测试），不包含 Shipping（正式发布）。

DivineBeastsArenaClient.Target.cs（客户端构建目标）、DivineBeastsArenaServer.Target.cs（服务器构建目标）和 DivineBeastsArenaEditor.Target.cs（编辑器构建目标）同时执行：
- Shipping（正式发布）：DisablePlugins.Add("GamePlatformDebug")（显式禁用调试插件）。
- 非 Shipping 且处于允许配置：EnablePlugins.Add("GamePlatformDebug")（显式启用调试插件）。

GamePlatformDebugClient（客户端调试模块）类型为 ClientOnly（仅客户端），Dedicated Server（专用服务器）不会依赖 Slate（界面）或客户端面板。

插件 CanContainContent=false（禁止插件内容资产），因此 V1（第一版）没有 Debug-only Asset（仅调试资产）进入 Cook（烘焙）的路径。

源代码仍以 #if !UE_BUILD_SHIPPING（非正式发布条件编译）做第二层保护。