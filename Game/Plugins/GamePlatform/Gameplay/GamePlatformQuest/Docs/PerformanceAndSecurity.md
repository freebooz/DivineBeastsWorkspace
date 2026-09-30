# PerformanceAndSecurity（性能与安全）

> 2026-09-30状态校正：本页保留旧方案/历史证据，旧DBAServer具体Quest HTTP/事件适配与后端交付宣称未在当前项目文件清单确认，不能作为现行验收。当前行为以[本轮整改说明](DesignRemediation-2026-09-30.md)、README与真实源码为准；本轮未执行数据库/Outbox/网络联调。

Quest Runtime 不启用 Tick；事件驱动按 `Player → EventType → Relevant Objectives`索引处理，不扫描全服任务。普通高频进度在 UE 内存立即推进并按短周期批量持久化，避免每个 Combat Event 单独访问数据库。

Recent Event 去重按 QuestId 有界缓存，最大数量来自 MaxRecentEventIds；PostgreSQL 关键幂等依赖 processed_event 主键、CompletionId 唯一记录、Revision CAS 和 Outbox EventId 唯一约束。

PlayerData 内部写路由要求受控内部令牌和玩家绑定 Header，但这不是最终生产服务身份模型。正式上线前必须接入 Online/Session 的服务器身份、密钥轮换、TLS/网络策略和审计。

Outbox Dispatcher 使用短事务 `FOR UPDATE SKIP LOCKED（跳过已锁行）`领取并设置 lease 后提交，网络 Publisher 调用不持有数据库行锁；失败记录 last_error 并延迟重试。具体 NATS Publisher 未实现/未运行。
