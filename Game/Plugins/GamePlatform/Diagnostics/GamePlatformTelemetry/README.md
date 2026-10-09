# GamePlatformTelemetry（游戏平台遥测插件）

正式路径：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry`。

UE（虚幻引擎）只有一个 `GamePlatformTelemetry（遥测双端运行模块）`，类型为 Runtime（双端运行时）。`UGamePlatformTelemetrySubsystem（平台遥测子系统）`使用 `UGameInstanceSubsystem（游戏实例子系统）`作用域，每个 Client/Dedicated Server/GameInstance（客户端/专用服务器/游戏实例）拥有独立 Context、Sequence、Buffer 与 Sink。

当前 UE 运行层已经建立结构化 Event/Metric/Batch/Context（事件/指标/批次/上下文）、Schema Registry（结构注册表）、Schema 驱动 Privacy/Type/Priority（隐私/类型/优先级）、Deterministic Sampling（确定性采样）、Event/Metric Rate Limit（事件/指标限流）、有界 Buffer、优先级 Drop（丢弃）、Null/Log/Network Sink（空/日志/网络输出器）、HTTP Transport（HTTP传输）、动态凭据请求头、有限断网重试/退避、按需 Flush（刷新）、关停预算和 Unreal Trace Channel（虚幻追踪通道）。DBAClient/DBAServer 已增加薄装配代码，但遥测失败始终 Fail-Open（失败开放），不会改变登录、世界、服务器 Ready 或业务状态。

当前仓库的 Go（Go语言）后端只存在 `Backend/internal/modules/telemetry/doc.go` 领域占位，尚未实现 Gateway `POST /telemetry/v1/batches`、GameServerControl `POST /internal/telemetry/v1/batches`、身份覆盖、后端 Schema/Privacy/Size 校验或真实持久/消息接入。因此 UE NetworkSink 已具备协议装配与重试能力，不等于端到端后端已经可用。

普通 Telemetry（遥测）仍明确不属于 PostgreSQL 业务权威，也不走 Transactional Outbox（事务外发）。文档中规划的 `telemetrynats（遥测NATS适配器）`、JetStream subject（消息流主题）`telemetry.events.v1` / `telemetry.metrics.v1` 当前尚未在真实 Backend 源码落地；后续实现时必须继续保证 NATS/Telemetry 故障只导致遥测拒绝/重试/丢弃，绝不能改变 Gameplay 或 Inventory/Quest/Commerce 等业务状态。

历史版本 Runner 记录使用 `D:\\UnrealEngine-5.8.0-release`。该历史版本源码曾真实完成 GamePlatformTelemetry 独立插件 UnrealEditor Win64 Development、UnrealGame Win64 Development/Shipping 构建，静态遥测架构门禁以及 Telemetry/DBAClient 客户端遥测装配定向编译均通过。上一版 AutomationHost 曾实际执行 7 个 `GamePlatform.Telemetry.*` 测试并全部成功；本轮新增 Metric 合并、隐私边界丢弃与账号认证代次隔离后的 Automation 重跑被 UE5.8 启动阶段 VisionOS SDK 校验提前阻断，新增测试目前只能证明已编译，不能宣称运行通过。有界 Fake Transport 故障注入不等于真实互联网断线；Go/NATS 后端仍未实现，Multi-PIE、真实网络故障、端到端 Ingest、Trace 运行和 Cook/Stage 仍需继续验收。

### 2026-10-09 自定义Sink生命周期整改

本轮锁定引擎源码为`F:/UnrealEngine-5.8.0-release`，仅修GI子系统的同步重入所有权：永久关闭门闩、候选Start清理、配置中嵌套重配明确拒绝、实际Sink代次、GetHealth/Submit后的出队与调度资格、受控最终Drain、异步完成一次门闩和一次性Ticker代次。账号/世界边界另外使用独立上下文操作身份：单纯换Sink仍完成EndSession/BeginSession/BeforeWorldTravel，真实后继会话/旅行/世界发布才使旧边界停止。保留公开中文API、外置默认/FVTableHelper构造与析构及原反射身份。

本轮架构静态门禁退出0，新增八个真实Subsystem/Sink Automation用例源码（含三种GetHealth换Sink边界与后继上下文接管）；没有运行UBT或UE Automation，不能沿用上述历史构建/测试结果宣称本轮通过。当前未实现的生产后端/网络/性能条件继续保留。详细合同见`Docs/SinksAndExporters.md`，独占证据为`Saved/Validation/PluginRemediation-2026-10-09/TelemetryLifecycle`。
