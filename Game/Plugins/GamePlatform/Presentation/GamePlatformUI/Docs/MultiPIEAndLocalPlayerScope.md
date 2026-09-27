# MultiPIEAndLocalPlayerScope（多PIE与本地玩家作用域）

Manager 继承 `ULocalPlayerSubsystem`，Screen Registry、Route Registry、Pending Request、ViewModel、Loading Service、Toast 和页面租约都存放在该 LocalPlayer 实例内。

这避免两个 LocalPlayer/PIE 客户端共享页面栈或加载状态。Root Layout 使用 `AddToPlayerScreen（添加到玩家屏幕）`而非全局 Viewport。

当前只有静态证据确认作用域设计；真实双 PIE 的页面、焦点、Travel、Loading Token 隔离测试因 Runner 无 UE5.8 工具链而状态为“未执行”。

测试脚本 `TestGamePlatformUIMultiPIE.ps1`在缺少 UE_ROOT 时返回非零“未执行”，不会伪报通过。
