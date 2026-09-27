# CommonUIIntegration（CommonUI集成）

`GamePlatformUIClient`直接依赖 CommonUI 和 CommonInput。`DefaultEngine.ini`已把 `GameViewportClientClassName`配置为 `/Script/CommonUI.CommonGameViewportClient`，让 CommonUI 输入先于普通 PlayerInput 路由。

正式页面基于 `UCommonActivatableWidget`，页面栈使用 `UCommonActivatableWidgetStack`。Root Layout 蓝图需要为 Screen/Modal/System/Loading/Debug 绑定对应 Stack，为 HUD/Notification 绑定 Overlay。

CommonUI Root 不在项目层重新设计第二套；`DivineBeastsUI`后续只能复用本平台 Root/Manager。

当前只完成源码与配置静态门禁；真正 Controller Data（控制器数据）、Back Action（返回动作）、Synthetic Cursor（模拟光标）和设备切换仍需 UE 运行验证。
