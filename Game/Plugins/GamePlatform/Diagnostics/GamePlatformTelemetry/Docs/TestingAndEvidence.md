# TestingAndEvidence（测试与证据）

静态入口：`Build/Validation/VerifyTelemetry.ps1（遥测验证入口）`。

分项脚本：`TestTelemetryUE.ps1（UE遥测静态门禁）`、`TestTelemetryBackend.ps1（后端接入门禁）`、`TestTelemetryNats.ps1（NATS边界门禁）`、`BenchmarkTelemetry.ps1（性能基准入口）`。

UE 自动化测试源码覆盖 Privacy/Schema、Deterministic Sampling、Rate Limit、Bounded Buffer、Priority Drop 和 Development 1000 Record Harness（开发1000条记录测试框架）。

Go 测试源码覆盖 Batch/Identity Override、Forbidden Commerce Receipt、High-cardinality Metric Label、Timestamp/Count Limits、Partial Rejection、NATS Failure Retryable 和 Request Rate Limit。

当前 Runner 无 Go/UE/NATS 工具链，因此源码测试存在不等于运行通过；实际 Go test、UE Automation、NATS Integration、Multi-PIE 均未执行。