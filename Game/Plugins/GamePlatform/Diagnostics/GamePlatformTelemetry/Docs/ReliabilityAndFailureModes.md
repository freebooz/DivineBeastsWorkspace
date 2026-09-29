# ReliabilityAndFailureModes（可靠性与故障模式）

Telemetry（遥测）失败必须 fail-open（对游戏业务开放）：Record 返回 Drop/RateLimit/Disabled 等轻量结果，调用方不得据此中断 Gameplay。

UE Buffer 满按优先级丢弃并计数；Network Sink 的 Pending Batch 有硬上限，不阻塞主线程。HTTP 4xx schema/auth 不重试；408/429/5xx/transport error 与 `ProcessRequest` 瞬时启动失败进入同一有限重试链，采用指数退避、确定性抖动、最大重试次数/年龄，并尊重 429 `Retry-After`。网络恢复后尚未超过预算的 Batch 会继续提交。

Network Sink 会保存并可撤销尚未触发的 Retry Ticker（重试定时任务）；Shutdown（关停）先禁止新重试并撤销待触发任务，对已经发出的 HTTP 给予 `ShutdownFlushBudgetSeconds` 非阻塞预算，预算到期才取消仍在飞请求并计入 Dropped。规划中的 NATS Publisher/503 retryable 行为当前后端尚未实现。

DBAClient/DBAServer 的遥测 Bootstrap 都是 Best Effort（尽力而为）：缺 Gateway/GameServerControl URL 或凭据时保留 NullSink/请求失败诊断，不能阻止客户端登录、服务器注册、Ready、Heartbeat 或 Gameplay。后端 Ingest 路由当前尚未落地。