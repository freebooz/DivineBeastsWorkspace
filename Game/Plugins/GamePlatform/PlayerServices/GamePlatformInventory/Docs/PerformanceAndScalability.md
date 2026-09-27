# PerformanceAndScalability（性能与伸缩）

客户端第一版全量 Snapshot 缓存于 LocalPlayerSubsystem（本地玩家子系统），写操作单飞，避免并行 ExpectedRevision 竞争；ViewModel/排序仅处理本地数据。

数据库用单玩家 InventoryState 行作为 Mutation 串行点，而不是锁全表。Item 有 player+definition 索引，Slot 有唯一约束，Operation 有 player+created_at索引。Grant 会批量查找可填充 Stack，并在同一事务分配空槽。

附件要求的 0/100/500 Items、Snapshot bytes、序列化耗时、Move/Grant事务耗时、并发冲突率和 Outbox 开销目前没有真实运行数据，因此不承诺容量/QPS。超出合理全量 Snapshot 后再设计 Pagination/Delta（分页/增量）。
