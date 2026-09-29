# ConfigurationAndRun（配置与运行）

UE Limits（限制）默认包括 MaxAttributes=24、MaxKeyLength=64、MaxStringLength=512、MaxContextStringLength=160、MaxEventBytes=8192、MaxBufferEvents=4096、MaxBufferBytes=4MiB、MaxBatchEvents=128、MaxBatchBytes=256KiB、MaxFlushBatchesPerPass=4、FlushInterval=5s、ShutdownFlushBudget=0.5s。FlushInterval 现在只是 Buffer 非空时的一次性兜底 Deadline，不是常驻周期 Tick。

Network Retry（网络重试）默认采用有限 Pending Batch、有限次数、指数退避、确定性抖动、Retry-After 和最大重试年龄。Pending 满时 Subsystem 不再提前从 Buffer 取批，待某个网络批次终态释放槽位后再继续 Drain，从而在断线期间保留有界背压。

当前 UE 真实环境入口包括客户端 `DIVINEBEASTS_GATEWAY_BASE_URL` / `DIVINEBEASTS_CONTENT_REVISION`，以及服务器 `GAMESERVERCONTROL_BASE_URL` / `TELEMETRY_SERVER_INTERNAL_TOKEN` / `GAME_SERVER_ID` / `GAME_SERVER_REGION_ID` 等；Secret 只在发送瞬间动态读取，不写通用客户端 ini/UAsset/C++ 常量。NATS_URL、NATS_CREDS_FILE、TELEMETRY_PSEUDONYMIZATION_KEY 属于未来 Backend/NATS 实现规划，当前尚未落地。

客户端/服务器 Telemetry URL 或 Token 缺失不得阻止登录、Gameplay、服务器注册、Ready 或 Heartbeat；当前 UE 端会保持 NullSink/失败开放。未来 Backend/NATS 实现同样必须保持缺配置时遥测不可用但普通宿主仍可启动。