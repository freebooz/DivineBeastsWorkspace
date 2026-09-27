# GamePlatformUI（游戏平台UI插件）

跨游戏可复用的 CommonUI（通用UI）/UMG（虚幻动态图形界面）基础框架。正式唯一代码模块为 GamePlatformUIClient（游戏平台UI客户端模块），宿主类型为 ClientOnly（仅客户端）。

核心能力：

- UGamePlatformUIManagerSubsystem（游戏平台UI管理子系统）：每个LocalPlayer（本地玩家）唯一UI门面。
- UGamePlatformUIScreen（页面基类）+ UGamePlatformUIScreenDefinition（页面定义）。
- CommonUI Layer Stack（界面层栈）：Screen/Modal/System/Loading/Debug使用Activatable Stack；HUD/Notification使用普通Overlay。
- UGamePlatformUIInputPolicy（UI输入策略）：通过FUIInputConfig管理Game/UI/All模式；仅Standalone允许本地Pause。
- UGamePlatformLoadingScreenService（加载界面服务）：多Token聚合；负进度统一为未知，>1夹紧到1，不伪造进度。
- HUD/Menu/Dialog/Toast基类、Route（路由）、普通事件驱动ViewModel（视图模型）。
- Screen/Route注册采用Fail Closed结构校验：Layer、Focus、Tag冲突、目标Screen、循环和本地软路径安全均被验证。
- 页面WidgetClass和PreloadAssets通过 GamePlatformData（平台数据）的 FGamePlatformAssetLoader（平台资产加载器）异步加载并保留Lease（租约）；没有同步大资源加载路径。
- Travel前取消旧异步请求、关闭非持久页面、清理Toast/HUD/Notification；LocalPlayer作用域为Multi-PIE隔离提供基础。
- PlatformWidgetVariants（平台控件变体）使用 FPlatformProperties::IniPlatformName（平台配置名）选择，默认/变体都经过安全校验。
- Accessibility Preferences（可访问性偏好）统一提供文本缩放、触控目标缩放、Reduced Motion（减少动态效果）、高对比度偏好和非颜色状态线索钩子；默认动画可按偏好降级为Instant（即时切换）。

正式直接依赖：GamePlatformCore（平台核心）、GamePlatformData（平台数据）、GamePlatformInputClient（平台输入客户端）、GamePlatformPresentationCore（平台表现核心），以及UE的UMG、Slate、SlateCore、CommonUI、CommonInput、GameplayTags。没有ModelViewViewModel（Beta MVVM）依赖，也禁止依赖DivineBeasts项目层、MobaPresentation、GamePlatformArena或后端服务。

CommonGameViewportClient已经配置。当前没有创建任何GamePlatformUI二进制 .uasset/.umap；Root Layout（根布局）、通用样式、模板和真实页面资源必须由Unreal Editor合法创建后再做运行验证。

当前静态状态：TestGamePlatformUI 71项通过；Travel 11项通过；三层架构1175项通过；DeveloperTools 483项通过；VerifyGamePlatformUI综合静态验证通过；39份专题+README共40类文档完整。

未执行：UE5.8 UBT/UHT编译反射、UE Automation实际运行、真实CommonUI输入、Client/Server Cook、Multi-PIE、Android/设备输入和性能基线。ManualReview（人工审查）保持未执行。

新增Go业务后端接口：不适用。新增Go微服务：不适用。

