# ScreenBase（页面基类）

`UGamePlatformUIScreen`继承 `UCommonActivatableWidget（CommonUI可激活控件）`，用于具有激活/反激活、Back（返回）、输入路由和焦点语义的正式页面。普通 HUD 子控件、Tooltip（提示）等不要求继承它。

页面激活时调用 ViewModel `BeginPage（开始页面）`，反激活时调用 `EndPage（结束页面）`并广播原生 Deactivated（反激活）事件，Manager 据此释放加载租约和暂停状态。

DesiredFocusWidgetName（期望焦点控件名）由 Definition 提供；找不到时回退到 CommonUI 基类策略。

Back 行为当前通过 `NativeOnHandleBackAction（原生返回处理）`实现关闭，但 CommonUI 页面是否注册为 Back Handler（返回处理器）仍需 Blueprint/资产侧按实际 Screen 配置并在真实输入测试中验证。
