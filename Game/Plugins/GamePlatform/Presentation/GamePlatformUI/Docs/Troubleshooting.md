# Troubleshooting（故障排查）

页面打不开：先检查 Screen Definition 是否注册、WidgetClass 是否有效、Required/Blocked Tags 是否允许、Root Layout 对应 Layer Stack 是否绑定。

页面加载后不出现：检查软类加载结果、CommonUI Stack 是否存在以及 Blueprint 是否继承 `UGamePlatformUIScreen`。

手柄无焦点：检查 DefaultFocusWidgetName、控件是否 Focusable（可聚焦）以及页面是否为当前栈顶。

输入被锁死：检查页面 `GetDesiredInputConfig`返回值，不要同时在业务 Widget 手工 `SetInputMode`。

跨服后旧页面回调出现：确认请求通过 Manager 打开且 Travel 前没有绕过 `PrepareForTravel`；检查 ViewModel Revision/PageGeneration。
