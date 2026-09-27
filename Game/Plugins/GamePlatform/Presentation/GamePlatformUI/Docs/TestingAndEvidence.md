# TestingAndEvidence（测试与证据）

当前 C++ Automation（自动化）源码覆盖：

- ViewModel Revision/PageGeneration（视图模型修订号/页面代次）和旧回调失效。
- Accessibility Preferences（可访问性偏好）归一化，以及 Reduced Motion（减少动态效果）对默认Transition（过渡）的降级策略。
- Loading Token（加载令牌）引用计数。
- Loading Progress（加载进度）>1夹紧到1，负值归一为-1未知。
- CommonUI InputPolicy（输入策略）GameOnly/UIOnly/GameAndUI三种模式。
- Screen Definition（页面定义）合法注册、重复拒绝、已加载抽象页面类拒绝、缺焦点拒绝、HUD误作Activatable Screen拒绝、外部WidgetClass路径拒绝。
- Route（路由）未注册目标拒绝、自指BackRoute拒绝、fallback/back循环拒绝、BackRoute查询、被引用Route注销保护与Screen引用保护。

当前 PowerShell（脚本）门禁覆盖：

- 唯一 GamePlatformUIClient（平台UI客户端）ClientOnly模块。
- 四项固定平台直接依赖和UE UI模块依赖。
- 12个107～118核心对象。
- 禁止项目/MOBA/后端硬依赖。
- 禁止HTTP、同步资产加载、项目UI自定义Tick。
- 正式UI Manager唯一、旧 Frontend/Plugins/GamePlatformUIClient 路径不存在。
- CommonGameViewportClient配置。
- Definition/Route验证、异步Asset Loader（资产加载器）、Lease（租约）、Travel生命周期。
- Server Target显式禁用GamePlatformUI且不引用GamePlatformUIClient。
- Development（二进制开发资源）在无Shipping排除证据时禁止出现。
- Client/Server Cook工件扫描入口。

本轮最终静态证据：TestGamePlatformUI 71项通过；Travel 11项通过；三层架构1175项通过；DeveloperTools 483项通过；VerifyGamePlatformUI综合静态状态通过。

TestGamePlatformUIMultiPIE（多编辑器实例测试）和TestGamePlatformUICook（烘焙测试）在没有真实环境/工件时返回“未执行”，综合验证会保留该状态，不把exit 2误报为失败或通过。

VerifyGamePlatformUI（平台UI综合验证）已经具备真实 UE5.8 执行路径：检测到 UE_ROOT 后会以独立 RunId 和日志目录构建 Editor/Client/Server 三个目标，并在 Editor 构建成功后调用 `UnrealEditor-Cmd.exe` 执行 `GamePlatform.UI.*` Automation。进程执行采用 `System.Diagnostics.Process（系统诊断进程）`获取真实 ExitCode（退出码），带超时，并且只终止本次启动的进程。

当前 Runner 没有 UE5.8 工具链，所以正式交付仍必须补本轮未能产生的编译日志、UHT反射、Automation实际运行、Client/Server Cook/Stage清单、Multi-PIE日志、PC/Android输入/焦点和性能数据。

