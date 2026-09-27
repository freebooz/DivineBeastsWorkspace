# BufferingAndBackpressure（缓冲与背压）

UE 使用显式 MaxBufferEvents、MaxBufferBytes、MaxBatchEvents、MaxBatchBytes、MaxEventBytes。

Buffer（缓冲）满时按照 Verbose → Normal → CriticalTelemetry 顺序寻找可丢弃记录；低优先级新记录不能驱逐更高优先级记录。所有 Drop（丢弃）都累计 Diagnostics（诊断）。

Record fast path（记录快速路径）不执行 HTTP、不加载 UObject 资产、不构建大 JSON。真正序列化在 HTTP Transport 提交 Batch 时完成。

Network Sink 的 Pending Batch、Retry 次数和 Retry Age 都有上限；NATS Client 重连次数和 reconnect buffer（重连缓冲）也显式有界。