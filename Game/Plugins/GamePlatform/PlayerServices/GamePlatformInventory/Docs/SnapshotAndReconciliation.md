# SnapshotAndReconciliation（快照与对账）

`UGamePlatformInventoryClientSubsystem（背包客户端子系统）`是 LocalPlayerSubsystem（本地玩家子系统），不使用进程全局 Singleton（单例）保存账号背包。登录后先 Loading（加载），Gateway 返回 Snapshot 后进入 Ready（就绪）。

所有写操作携带 `ExpectedRevision（预期修订号）`。后端发现 RevisionConflict（修订冲突）后，客户端进入 Reconciling（对账中）并刷新完整 Snapshot；旧 Revision 不会覆盖新 Revision。第一版采用全量 Reconcile（全量对账），没有伪造 Delta（增量）已投入运行。

Account Switch（账号切换）会先提升 `AccountGeneration（账号代次）`，使旧回调立即失效，再调用 Transport `CancelAllRequests（取消全部请求）`物理取消在途 HTTP；旧账号迟到响应不能写入新账号缓存。

网络结果未知时 Pending Operation（待处理操作）不会生成新的 OperationId。用户显式重试先调用 Gateway `GET /v1/inventory/operations/{operationId}`查询持久化结果：若已提交则直接应用返回 Snapshot；只有后端明确返回 OperationNotFound（操作未找到）时，才复用原 OperationId 重发一次。不会自动无限重试写操作。
