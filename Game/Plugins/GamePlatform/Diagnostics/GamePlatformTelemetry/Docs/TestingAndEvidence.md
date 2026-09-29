# TestingAndEvidence（测试与证据）

当前真实静态入口：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Tests/Scripts/TestTelemetryArchitecture.ps1（遥测架构/性能门禁）`。此前文档列出的 `Build/Validation/VerifyTelemetry.ps1`、`TestTelemetryUE.ps1`、`TestTelemetryBackend.ps1`、`TestTelemetryNats.ps1`、`BenchmarkTelemetry.ps1` 当前仓库不存在，不再作为已实现入口描述。

UE 自动化测试覆盖 Privacy/Schema 类型约束与 Freeze、Deterministic Sampling、Rate Limit、Bounded Buffer、Priority Drop、Context 变化自动切 Batch、Development 1000 Record Harness，以及断线后有限退避重试恢复、Shutdown 取消未触发重试。静态门禁额外检查按需一次性 Flush、重试撤销、Context 去重、动态凭据、Payload 硬上限、Schema Freeze、HeadIndex 批次消费与 Public 依赖边界。

当前真实 Backend 只有 `Backend/internal/modules/telemetry/doc.go` 占位，没有上述 Go Ingest/NATS 测试源码，因此不能声称后端测试已覆盖 Batch/Identity/NATS 等场景。

当前 Runner 已真实执行 UE5.8 BuildPlugin：UnrealEditor Win64 Development、UnrealGame Win64 Development/Shipping 均在本轮优化后通过。独立 AutomationHost 已真实发现并执行 7 个 `GamePlatform.Telemetry.*` 自动化测试，ReconnectRecovery、ShutdownCancelsRetry、Development1000Records、BoundedBuffer、ContextBatchBoundary、PrivacyAndSchema、SamplingAndRateLimit 全部 Result=Success。这里的断线恢复是可控 Fake Transport 故障注入，不等于真实网卡断开/恢复、Gateway/NATS 端到端验证；Multi-PIE、真实网络故障、Backend/NATS Integration、Client/Server Cook 仍需分别形成证据。