# TelemetryIntegration（遥测集成）

GamePlatformDebug（调试插件）读取现有 GamePlatformTelemetry（遥测插件），不复制第二套 Telemetry（遥测）系统，也不修改 Telemetry Schema（遥测结构）。

当前真实读取：
BufferDepth（缓冲深度）、BufferBytes（缓冲字节）、RecordedTotal（记录总数）、DroppedCounts（丢弃计数）、SampledOut（采样丢弃）、RateLimited（限流计数）、Enabled（启用状态）、SinkHealth（输出器健康）、PendingNetworkBatches（待发送网络批次）、Submitted/Failed Batches（已提交/失败批次）、LastFlush（最近刷新）、SinkLastSuccess/SinkLastFailure（输出器最近成功/失败）和 SinkLastError（最近错误）。

Telemetry 已通过 `FGamePlatformTelemetryDiagnostics（平台遥测诊断快照）`公开安全只读健康摘要，不向 Debug 暴露 NetworkSink/Transport 具体对象。Sampling（采样策略）仍按各 Schema 定义，不提供运行期全量枚举，因此 Debug 中该项继续显示 N/A。

gp.Debug.Telemetry.Flush（遥测刷新命令）仅在非 Shipping（正式发布）模块中存在，并受 gp.Debug.AllowMutating=1（允许低风险可变命令）额外开关保护。