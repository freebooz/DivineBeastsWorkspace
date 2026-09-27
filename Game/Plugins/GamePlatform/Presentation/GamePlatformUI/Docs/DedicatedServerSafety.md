# DedicatedServerSafety（专用服务器安全）

GamePlatformUI.uplugin（游戏平台UI插件描述）只声明 GamePlatformUIClient（游戏平台UI客户端模块），Type固定为ClientOnly（仅客户端）。

统一 DivineBeastsArenaServer.Target.cs（神兽联盟服务器构建目标）当前显式 DisablePlugins.Add("GamePlatformUI")，并且不引用 GamePlatformUIClient 模块。该显式禁用是ClientOnly模块边界之外的额外防护，避免服务器目标误带UI插件内容。

Server Cook/Stage（服务器烘焙/暂存）还必须验证没有：

- GamePlatformUIClient。
- /GamePlatformUI/ UI内容。
- Widget Blueprint（控件蓝图）。
- CommonUI内容。
- UI字体、图标。
- Content/Client/Development开发验证资源。

TestGamePlatformUICook.ps1（平台UI烘焙门禁）已经支持扫描真实Client/Server工件目录；当前没有工件目录和UE5.8工具链，所以Server Cook状态仍为“未执行”。

代码层不使用 #if UE_SERVER 掩盖错误模块依赖；正确隔离来自ClientOnly宿主、Target禁用、模块依赖和真实Cook/Stage共同证据。

