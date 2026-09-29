# SnapshotAndReconciliation（快照与对账）

`UGamePlatformInventoryClientSubsystem（背包客户端子系统）`是 LocalPlayerSubsystem（本地玩家子系统）。初始化时订阅 Online 认证事件：Authenticated（已认证）自动配置背包并加载 Snapshot；Refreshing（刷新认证）保留当前缓存与 Pending；LoggingIn/LoggingOut/LoggedOut/Failed 会清理旧账号状态。账号切换提升 `AccountGeneration`，旧账号回调立即失效。

快照另外使用 `SnapshotRequestGeneration（快照请求代次） + bSnapshotRequestInFlight（快照在途标记）`。同一账号同一时刻只允许一个完整快照请求，迟到快照不能覆盖新请求，也不能把已经成功的 Mutation 状态重新打成 Error。

所有写操作携带 OperationId 与 ExpectedRevision。RevisionConflict 时设置专用 `bConflictSnapshotReconcile` 并进入 Reconciling，只有这一条内部全量对账路径允许在存在 Pending Operation 时刷新并清理旧 Pending；用户普通 Refresh 在 Pending 未决期间直接拒绝，避免误把结果未知操作当成失败清除。

网络/超时/响应解析异常等结果未知错误会保留原 Pending Operation。显式 `RetryPendingOperation` 先查询 `GET /v1/inventory/operations/{operationId}`：如果已持久化则应用返回快照；只有后端明确返回 `INVENTORY_OPERATION_NOT_FOUND` 才复用同一 OperationId 重发，不生成新键、不自动无限重试。

确定性的业务错误，例如 SlotOccupied、InvalidQuantity、StackLimitExceeded，会清理 Pending 并恢复 Ready，同时通过 `GetLastError()`保留稳定错误供 UI 展示。Unauthorized 会阻断客户端，等待 Online 认证状态处理。

`ApplySnapshot` 现在校验聚合 Revision、容器重复/容量、ItemInstanceId 重复、Container+Slot 重复、Quickbar 槽位重复以及 Quickbar 悬空引用。当前仍采用全量 Snapshot；在真实数据证明有必要前不引入伪 Delta 或 FastArraySerializer。
