# TelemetryIntegration（遥测集成）

GamePlatformDebug（调试插件）读取现有 GamePlatformTelemetry（遥测插件），不复制第二套 Telemetry（遥测）系统，也不修改 Telemetry Schema（遥测结构）。

当前真实读取：
BufferDepth（缓冲深度）、BufferBytes（缓冲字节）、RecordedTotal（记录总数）、DroppedCounts（丢弃计数）、SampledOut（采样丢弃）、RateLimited（限流计数）。

Telemetry 当前没有公开 Enabled/Sampling/Sinks/LastFlush/BackendSinkHealth（启用/采样/输出器/最近刷新/后端输出健康）的安全只读聚合接口，因此这些字段显示 N/A（不可用）。

gp.Debug.Telemetry.Flush（遥测刷新命令）仅在非 Shipping（正式发布）模块中存在，并受 gp.Debug.AllowMutating=1（允许低风险可变命令）额外开关保护。