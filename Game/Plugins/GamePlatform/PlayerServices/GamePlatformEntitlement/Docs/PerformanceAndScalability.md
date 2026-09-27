# PerformanceAndScalability（性能与扩展）

客户端 Snapshot 缓存避免每帧访问后端；HasEntitlement/HasAny/HasAll 都在内存数组上完成。

PlayerData 提供 CheckEntitlements（批量权益检查），一次读取有效 Snapshot 后完成最多 256 个 EntitlementId 的授权布尔判定，避免逐项数据库往返。

当前没有 0/10/100/1000 Entitlement 的真实序列化、数据库查询、Grant/Revoke 事务性能数据，因此不承诺容量指标。