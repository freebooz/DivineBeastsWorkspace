# ReliabilityAndFailureModes（可靠性与故障模式）

Telemetry（遥测）失败必须 fail-open（对游戏业务开放）：Record 返回 Drop/RateLimit/Disabled 等轻量结果，调用方不得据此中断 Gameplay。

UE Buffer 满按优先级丢弃并计数；Network Sink 队列满直接丢批次，不阻塞主线程。HTTP 4xx schema/auth 不无限重试；408/429/5xx/transport error 可有限重试；429 尊重 Retry-After。

NATS Publisher 使用有限 reconnect 次数和 1MiB reconnect buffer。NATS 发布失败使 Ingest 返回 retryable 503；后端不把普通遥测无限积压到内存或数据库。

GameServerControl 缺 Telemetry 配置仍可启动，只是不注册遥测 Ingest 路由。