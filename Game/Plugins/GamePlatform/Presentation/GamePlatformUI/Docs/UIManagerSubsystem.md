# UIManagerSubsystem（UI管理子系统）

`UGamePlatformUIManagerSubsystem`继承 `ULocalPlayerSubsystem（本地玩家子系统）`，每个 LocalPlayer 自动拥有独立实例，避免 Multi-PIE（多玩家编辑器运行）之间共享页面、加载 Token 或 ViewModel。

职责包括：Screen/Route 注册、Root Layout 安装、异步页面打开/取消、资源 Lease（租约）持有、页面关闭释放、HUD/Toast 挂载、Standalone（单机）暂停策略、LoadMap 前清理。

业务代码不得直接 `CreateWidget + AddToViewport`绕过门面。Manager 内部创建 Root Layout 属于框架装配，不是业务页面捷径。

所有异步打开使用唯一 RequestId（请求编号）；取消后即使底层回调迟到，也因 PendingRequests（待处理请求）已不存在而被忽略。

2026-10-09 补充：当平台专属 Widget Variant（控件变体）异步加载失败、类为抽象类或类型不匹配时，页面管理器在同一个 RequestId（请求编号）下最多重新异步加载一次共享默认 Widget；失败则发送唯一终态错误并释放请求与资源租约。取消、切换地图或子系统销毁时均清理回退标识。该通用机制不意味着项目层不存在的视觉资产已交付。
