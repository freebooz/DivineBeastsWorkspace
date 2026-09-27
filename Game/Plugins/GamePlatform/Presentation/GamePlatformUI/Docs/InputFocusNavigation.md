# InputFocusNavigation（输入、焦点与导航）

每个正式 Activatable Screen（可激活页面）应提供 DefaultFocusWidgetName（默认焦点控件名），页面激活后由 CommonUI 请求合理焦点；Gamepad（手柄）页面无焦点视为缺陷。

输入模式由 `GetDesiredInputConfig`返回，不与手工 `SetInputMode`混用。Modal 关闭后由 CommonUI 栈恢复前页面及输入上下文。

必须在真实 UE 环境验证 Keyboard/Mouse（键鼠）、Gamepad（手柄）、Touch（触控）、Back（返回）和焦点恢复。

当前源码对这些路径已建立基础接口，但运行证据状态仍是“未执行”。
