# UIManagerSubsystem（UI管理子系统）

`UGamePlatformUIManagerSubsystem`继承 `ULocalPlayerSubsystem（本地玩家子系统）`，每个 LocalPlayer 自动拥有独立实例，避免 Multi-PIE（多玩家编辑器运行）之间共享页面、加载 Token 或 ViewModel。

职责包括：Screen/Route 注册、Root Layout 安装、异步页面打开/取消、资源 Lease（租约）持有、页面关闭释放、HUD/Toast 挂载、Standalone（单机）暂停策略、LoadMap 前清理。

业务代码不得直接 `CreateWidget + AddToViewport`绕过门面。Manager 内部创建 Root Layout 属于框架装配，不是业务页面捷径。

所有异步打开使用唯一 RequestId（请求编号）；取消后即使底层回调迟到，也因 PendingRequests（待处理请求）已不存在而被忽略。

页面暂时失活只调整显示暂停；正常离开CommonUI WidgetList后发布OnScreenClosed并ReleaseResources。Root替换/退出主动撤销实例所有权时则先撤账并通知，再拆旧Root，此时旧WidgetList可能仍保留页面。页面SoftClass与PreloadAssets统一由Data普通资源租约加载。

2026-10-09 补充：当平台专属 Widget Variant（控件变体）异步加载失败、类为抽象类或类型不匹配时，页面管理器在同一个 RequestId（请求编号）下最多重新异步加载一次共享默认 Widget。回退仍通过Data申请普通资源租约，同时重新声明全部PreloadAssets；旧变体租约先撤销，不能保留第二套StreamableHandle所有权。默认类或必需依赖失败则发送唯一终态错误并释放资源。取消、切换地图、构造撤回或子系统销毁时均清理回退标识；同步构造取消仍须等待CommonUI退栈后才释放本次租约。该通用机制不意味着项目层不存在的视觉资产已交付。

2026-10-09具体实例关闭补充：兼容的`OnScreenClosed`仍只携带ScreenId（内容身份），同一ID允许多个页面实例。新增GT只读`IsScreenOwnedByStack(Screen, ExpectedStack)`（具体页面是否仍归原层栈）查询：两个参数必须有效，Manager必须未关闭/替换，ScreenStacks必须仍登记此精确实例到原栈且CommonUI WidgetList仍含它，才返回true。暂失活不改变结果；无账本、旧Root撤账、空值或换栈均为false。调用方用它区分“关闭其他同ID页”与“自己的实例已经撤销”，不暴露TMap/Data租约，不改变拥有，不增加反向项目依赖或修改Root清理次序。该纯C++方法与Tests下中立friend不扩展通用Interface纯虚函数；消费插件需要重编译。

受控Tests仅注入真实ScreenStacks具体实例前提，随后走真实Manager.CloseScreen或Deinitialize→ClearScreenOwnership→OnScreenClosed。没有伪造Data成功租约，也未声明完整异步Open/蓝图Root创建/视觉通过。真实CommonUI栈、同ID两个Native页面和关闭期旧栈仍持页的回归源码位于竞技客户端Tests；平台Public头只认识中立测试访问名，不引用上层测试类型。
