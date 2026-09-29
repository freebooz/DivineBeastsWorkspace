# TestingAndEvidence（测试与证据）

当前真实静态入口：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Tests/Scripts/TestTelemetryArchitecture.ps1（遥测架构/性能门禁）`。此前文档列出的 `Build/Validation/VerifyTelemetry.ps1`、`TestTelemetryUE.ps1`、`TestTelemetryBackend.ps1`、`TestTelemetryNats.ps1`、`BenchmarkTelemetry.ps1` 当前仓库不存在，不再作为已实现入口描述。

UE 自动化测试覆盖 Privacy/Schema 类型约束与 Freeze、Deterministic Sampling、Rate Limit、Bounded Buffer、Priority Drop、Context 变化自动切 Batch、Counter/Gauge 合并、隐私边界旧会话记录丢弃、Development 1000 Record Harness，以及断线后有限退避重试恢复、Shutdown 取消未触发重试。静态门禁额外检查按需一次性 Flush、重试撤销、Context 去重、Metric 合并、隐私边界丢弃、动态凭据、Payload 硬上限、Schema Freeze、HeadIndex 批次消费与 Public 依赖边界。

当前真实 Backend 只有 `Backend/internal/modules/telemetry/doc.go` 占位，没有上述 Go Ingest/NATS 测试源码，因此不能声称后端测试已覆盖 Batch/Identity/NATS 等场景。

本轮最终源码已真实通过 UE5.8 `BuildPlugin`：UnrealEditor Win64 Development、UnrealGame Win64 Development、UnrealGame Win64 Shipping 均 `BUILD SUCCESSFUL`；Telemetry CoreTests、NetworkSink、Subsystem、Transport、Telemetry Unity 模块以及 DBAClient 的 `DivineBeastsClientTelemetryBootstrapSubsystem` 均使用当前 Client 目标生成的正式响应文件定向编译通过。静态架构门禁同时通过。完整 `DivineBeastsArenaClient` 构建仍被本任务外的 UI/Inventory/Online/Session 现有编译错误阻断，Dedicated Server Bootstrap 的定向编译也被 GamePlatformServer 现有 generated.h/UHT 问题提前阻断，因此不能声称完整 Client/Server 目标通过。

上一版独立 AutomationHost 曾真实执行 7 个 `GamePlatform.Telemetry.*` 测试并全部成功；本轮新增 `MetricCoalescing`、`PrivacyBoundaryDiscard` 和账号认证代次隔离后，最终 Automation 重跑被 UE5.8 启动阶段强制 `ValidatePlatforms -AllPlatforms` 的 VisionOS SDK 缺失提前阻断，测试队列没有开始执行。因此当前新增测试只具备“源码已编译”证据，不能描述为已运行通过。这里的 ReconnectRecovery 仍是可控 Fake Transport 故障注入，不等于真实网卡断开/恢复、Gateway/NATS 端到端验证；Multi-PIE、真实网络故障、Backend/NATS Integration、Client/Server Cook 仍需分别形成证据。