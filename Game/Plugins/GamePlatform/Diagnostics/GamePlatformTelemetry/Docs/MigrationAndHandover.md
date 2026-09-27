# MigrationAndHandover（迁移与交接）

本轮没有 PostgreSQL Migration（数据库迁移），这是设计结果：普通 Telemetry 不应变成业务持久权威。

Go 增加 `github.com/nats-io/nats.go v1.50.0（NATS Go客户端）`依赖；当前 Runner 没有 Go 工具，因此 go.sum 未通过 go mod tidy（模块整理）生成运行证据。正式构建环境需执行依赖解析和 Go tests。

未来新增 Event/Metric 必须同步：UE Schema Registry、Go Registry、Shared Contract、Privacy Class、Sampling/Rate Limit、测试和文档。

若未来接 OpenTelemetry/ClickHouse/Prometheus 等下游，应新增 Exporter/Consumer，不修改当前业务 Outbox 语义。

本轮停止在 GamePlatformTelemetry；下一插件 GamePlatformDebug（游戏平台调试插件）未实现。