# API（公开接口）

主要公开入口是 `UGamePlatformUIManagerSubsystem（游戏平台UI管理子系统）`。上层通过它注册 `UIScreenDefinition（页面定义）`、`UIRouteDefinition（路由定义）`，安装 Root Layout（根布局），异步打开/取消/关闭页面，并获取 Loading Service（加载服务）。

`UGamePlatformUIScreen（页面基类）`暴露 ScreenId（页面编号）、ViewModel（视图模型）和 CloseScreen（关闭页面）；输入配置由 CommonUI 的 `GetDesiredInputConfig（期望输入配置）`统一返回。

`UGamePlatformViewModelBase（视图模型基类）`提供 Revision（修订号）、PageGeneration（页面代次）和 `IsCallbackCurrent（回调新鲜度检查）`，用于丢弃页面关闭后或新一轮请求前的迟到回调。

`UGamePlatformLoadingScreenService（加载界面服务）`采用 Token（令牌）聚合；未知进度使用负值，不产生伪造百分比。

`FGamePlatformUIAccessibilityPreferences（UI可访问性偏好）`由 `UGamePlatformUIManagerSubsystem` 保存并广播变化，提供文本缩放、触控目标缩放、减少动态效果、高对比度偏好和非颜色状态线索钩子；`ResolveTransition（解析过渡）`负责把 Reduced Motion 下的默认动画降级为即时切换。
