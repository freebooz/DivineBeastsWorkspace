# SnapshotAndReconciliation（快照与对账）

客户端 Snapshot 只包含当前 Effective Entitlement，不下发完整 Grant 审计历史。字段包含 Revision、GeneratedAt 和有效权益条目。

`GamePlatformEntitlementClient（权益客户端模块）`使用 LocalPlayerSubsystem（本地玩家子系统）缓存 Snapshot，旧 Revision 不覆盖较新 Revision。当前第一版采用全量 Snapshot Refresh（快照刷新），没有伪造实时 Delta（增量）已经投入运行。

Account Switch（账号切换）提升 AccountGeneration（账号代次）、取消在途请求、清空旧 Snapshot。旧账号迟到回调因 Generation 不匹配而被丢弃。