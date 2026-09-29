# ManualReview（人工审查）

状态：未执行。AI 未代签。

人工审查至少覆盖：Client/Server UE Build、Client/Server Cook、2 PIE 隔离、后台线程记录、Schema/Privacy、Metric Label、Deterministic Sampling、Rate Limit、Buffer/Drop/Batch、Sink Failure、Retry/Backoff、Account Switch、World Travel、Unreal Trace/Bookmark、Gateway Auth、Server Auth/Registry、Body/Rate Limits、NATS JetStream 实发、NATS outage/recovery、固定 subjects、无 Telemetry Outbox、Commerce raw receipt 拒绝、业务审计边界、1/10/100/1000 events/sec 基准和 Shutdown Budget。

当前 Runner 已具备 UE5.8：本轮已真实完成 Telemetry 独立插件 Editor Development 与 Game Development/Shipping 构建，静态架构/性能门禁也已执行。Go/NATS 后端仍未实现；UE Automation、Multi-PIE、真实断网/恢复、Client/Server 完整工程、Cook、Trace 和端到端 Ingest 仍需单独验收。