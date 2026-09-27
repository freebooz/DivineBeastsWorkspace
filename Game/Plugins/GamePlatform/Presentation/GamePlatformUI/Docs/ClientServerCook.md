# ClientServerCook（客户端/服务器烘焙）

Client（客户端）目标应包含 GamePlatformUIClient（平台UI客户端模块）及被正式 Screen Definition（页面定义）软引用的UI资源；Content/Client/Development（开发验证内容）必须排除Shipping。

Dedicated Server（专用服务器）目标显式禁用 GamePlatformUI 插件，不得链接 GamePlatformUIClient。Server Cook/Stage不得包含Widget Blueprint、CommonUI内容、UI字体/图标或 /GamePlatformUI/ 内容。

TestGamePlatformUICook.ps1 当前采用“真实工件扫描”策略：

- 未提供ClientArtifactRoot/ServerArtifactRoot、路径不存在或工件目录为空：状态为“未执行”，exit 2。
- Client（客户端）工件既扫描Development内容泄漏，也必须在文件名或可读的Receipt/Target/Modules/Manifest（收据/目标/模块/清单）中找到 `GamePlatformUIClient（游戏平台UI客户端模块）`正向证据；只有“没发现泄漏”但找不到正式模块包含证据时仍是“未执行”，不能误报通过。
- Server（服务器）工件扫描GamePlatformUIClient、/GamePlatformUI/、WidgetBlueprint、CommonUI、Fonts、Icons等UI泄漏。
- 只有非空真实工件存在，客户端正向包含证据成立，且客户端/服务器均无相应泄漏时，Cook/Stage检查才允许标“通过”。

当前Runner没有真实Client/Server Cook工件，因此Client Cook与Server Cook均为“未执行”。静态ClientOnly和Server Target禁用不能替代真实Cook证据。

