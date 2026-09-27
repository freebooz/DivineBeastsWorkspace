# ConfigurationAndRun（配置与运行）

UE Limits（限制）默认包括 MaxAttributes=24、MaxKeyLength=64、MaxStringLength=512、MaxEventBytes=8192、MaxBufferEvents=4096、MaxBufferBytes=4MiB、MaxBatchEvents=128、MaxBatchBytes=256KiB、FlushInterval=5s、ShutdownFlushBudget=0.5s。

Network Retry（网络重试）默认采用有限 Pending Batch、有限次数、指数退避、抖动和最大重试年龄。

后端环境样例增加 NATS_URL、NATS_CREDS_FILE、TELEMETRY_PSEUDONYMIZATION_KEY、TELEMETRY_SERVER_INTERNAL_TOKEN、GAMESERVERCONTROL_LISTEN_ADDR。Secret 只由后端/服务器部署环境注入，不写通用客户端 ini/UAsset/C++ 常量。

NATS 或 Telemetry Token 缺失不得阻止 Gateway/GameServerControl 普通宿主启动；缺失时遥测接入不可用或不注册。