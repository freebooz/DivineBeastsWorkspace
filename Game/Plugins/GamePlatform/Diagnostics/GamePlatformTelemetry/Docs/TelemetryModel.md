# TelemetryModel（遥测模型）

TelemetryEvent（遥测事件）包含 EventId、EventName、SchemaVersion、TimestampUtc、MonotonicTimestamp、Sequence、Priority、Context、Attributes。

TelemetryMetric（遥测指标）支持 Counter/Gauge/Histogram/Duration（计数器/仪表值/直方图/时长），数值使用 double 仅表示观测测量，不承载货币或 XP 权威。

TelemetryBatch（遥测批次）包含 BatchId、SchemaVersion、SourceContext、CreatedAtUtc、Events、Metrics 和 DroppedSinceLastBatch。

Priority 固定为 verbose/normal/critical_telemetry（详细/普通/关键遥测）；CriticalTelemetry 仍不是 Business Audit（业务审计）。