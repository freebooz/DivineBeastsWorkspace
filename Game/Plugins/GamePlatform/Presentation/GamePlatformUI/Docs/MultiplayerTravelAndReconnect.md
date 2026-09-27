# MultiplayerTravelAndReconnect（多人跨服与重连UI）

UGamePlatformUIManagerSubsystem（平台UI管理子系统）监听 FCoreUObjectDelegates::PreLoadMapWithContext（地图加载前回调）。只有回调WorldContext与当前LocalPlayer World匹配时才执行UI Travel（切服）清理。

PrepareForTravel（切服前准备）当前执行：

- 取消全部Pending Screen Open（待处理页面打开）并取消对应异步加载。
- 关闭所有未显式 bSurvivesTravel（跨地图保留）的Activatable Screen（可激活页面）。
- 保留显式LocalPlayer-global页面，但调用方仍不得让它持有旧World Actor引用。
- 过期并移除Toast。
- 清理HUD Overlay（HUD覆盖层）和Notification Overlay（通知覆盖层），避免World-scoped普通控件跨世界泄漏。
- 提升Generation（代次），使旧异步结果无法继续作为当前页面生命周期使用。
- Loading Token服务保留，以支持跨服期间展示真实加载事务。

bSurvivesTravel默认false，必须显式启用。

Reconnect（重连）状态、Endpoint和Server选择仍由Application/Session Owner（应用/会话所有者）提供；UI只能显示状态与发出重试/取消Intent。真实ClientTravel、旧回调丢弃和重连E2E当前均未执行。

