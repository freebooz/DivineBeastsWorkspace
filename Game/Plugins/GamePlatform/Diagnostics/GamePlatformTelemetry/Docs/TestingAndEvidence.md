# TestingAndEvidence（测试与证据）

当前真实静态入口：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Tests/Scripts/TestTelemetryArchitecture.ps1（遥测架构/性能门禁）`。此前文档列出的 `Build/Validation/VerifyTelemetry.ps1`、`TestTelemetryUE.ps1`、`TestTelemetryBackend.ps1`、`TestTelemetryNats.ps1`、`BenchmarkTelemetry.ps1` 当前仓库不存在，不再作为已实现入口描述。

UE 自动化测试源码覆盖 Privacy/Schema 类型约束、Deterministic Sampling、Rate Limit、Bounded Buffer、Priority Drop、Context 变化自动切 Batch，以及 Development 1000 Record Harness（开发1000条记录测试框架）。静态门禁额外检查按需一次性 Flush、重试撤销、Context 去重、动态凭据、Payload 硬上限与 Public 依赖边界。

当前真实 Backend 只有 `Backend/internal/modules/telemetry/doc.go` 占位，没有上述 Go Ingest/NATS 测试源码，因此不能声称后端测试已覆盖 Batch/Identity/NATS 等场景。

当前 Runner 已真实执行 UE5.8 BuildPlugin：UnrealEditor Win64 Development、UnrealGame Win64 Development/Shipping 均曾在本轮优化后通过。最终复验若遇到 UBT 全局互斥锁，只记录为并发构建阻断。UE Automation、真实断网/恢复、Multi-PIE、Backend/NATS Integration 仍需分别形成运行证据。