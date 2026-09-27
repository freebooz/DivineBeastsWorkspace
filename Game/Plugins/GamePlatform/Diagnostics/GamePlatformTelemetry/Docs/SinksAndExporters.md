# SinksAndExporters（输出器与导出器）

公共接口 `IGamePlatformTelemetrySink（遥测输出器接口）`提供 Start、SubmitBatch、Flush、Shutdown、GetHealth。

已实现 NullSink（空输出器）、Development LogSink（开发日志输出器）、NetworkSink（网络输出器）。LogSink 只输出 BatchId/数量/Drop，不打印 Attributes 或 Secret（敏感属性/密钥）。

NetworkSink 通过中立 `IGamePlatformTelemetryTransport（遥测传输接口）`工作，采用 bounded retry（有界重试）、exponential backoff（指数退避）、deterministic jitter（确定性抖动）、Retry-After 和 MaxPendingBatches。

未来可适配 IAnalyticsProvider（UE分析提供器）或其它 Exporter，但公共 API 不绑定第三方厂商。