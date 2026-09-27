# NATSAndStorageBoundary（NATS与存储边界）

普通遥测直接通过 `telemetrynats（遥测NATS适配器）`发布 JetStream（消息流），固定 subject 为 `telemetry.events.v1`和`telemetry.metrics.v1`。

Subject（主题）禁止拼 PlayerId。每个 Batch 的 Event/Metric 发布使用稳定 Nats-Msg-Id（NATS消息编号），为 JetStream 去重提供依据；实际去重效果仍依赖目标 Stream（消息流）配置的 duplicate window（重复窗口）。

Telemetry 不写 gameplatform_outbox（业务事务外发表），因为普通遥测可采样、可丢失、高频且非业务权威。

下游 ClickHouse/Elastic/Prometheus/Grafana（分析库/搜索/监控/看板）本轮未建设，状态为未执行。