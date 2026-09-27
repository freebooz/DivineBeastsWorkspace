# GamePlatformTelemetry（游戏平台遥测插件）

正式路径：`Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry`。

UE（虚幻引擎）只有一个 `GamePlatformTelemetry（遥测双端运行模块）`，类型为 Runtime（双端运行时）。`UGamePlatformTelemetrySubsystem（平台遥测子系统）`使用 `UGameInstanceSubsystem（游戏实例子系统）`作用域，每个 Client/Dedicated Server/GameInstance（客户端/专用服务器/游戏实例）拥有独立 Context、Sequence、Buffer 与 Sink。

第一版已经建立结构化 Event/Metric/Batch/Context（事件/指标/批次/上下文）、Schema Registry（结构注册表）、Privacy Filter（隐私过滤）、Deterministic Sampling（确定性采样）、Rate Limit（限流）、有界 Buffer、优先级 Drop（丢弃）、Null/Log/Network Sink（空/日志/网络输出器）、HTTP Transport（HTTP传输）、Unreal Trace Channel（虚幻追踪通道）和项目层 Client/Server Bootstrap（客户端/服务端启动适配器）。

Go（Go语言）领域位于 `Backend/gameplatform/telemetry`。客户端通过 Gateway `POST /telemetry/v1/batches`认证接入；Dedicated Server 通过现有 GameServerControlService `POST /internal/telemetry/v1/batches`接入。后端再次执行 Schema/Privacy/Size/Timestamp/Metric Label（结构/隐私/尺寸/时间/指标标签）校验，并覆盖不可信身份字段。

普通 Telemetry（遥测）不写 PostgreSQL、不走业务 Transactional Outbox（事务外发），而由 `telemetrynats（遥测NATS适配器）`直发固定 JetStream subject（消息流主题）`telemetry.events.v1`和`telemetry.metrics.v1`。NATS/Telemetry 故障只能导致遥测拒绝/重试/丢弃，不能改变 Gameplay 或 Inventory/Quest/Commerce 等业务状态。

当前 Runner（运行器）没有 Go、NATS、UE5.8 运行工具链，真实 Go test、NATS publish、Multi-PIE、Trace、Client/Server Build/Cook 和性能基准均保持“未执行”。
