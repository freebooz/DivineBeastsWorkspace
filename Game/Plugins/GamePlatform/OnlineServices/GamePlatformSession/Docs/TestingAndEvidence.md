# Session内核测试证据（任务切换检查点）

日期2026-09-21；后续用户切换到第六Loading插件，本文件只固化此前已实际完成的验证，不扩大新范围。

原生Session状态测试Debug/Release各1个CTest集合通过，生产实现位于`Private/State/SessionConnectionState.*`。证据`Saved/Validation/GamePlatformSession/Native/Testing/Temporary/LastTest.log`为末次Release结果；不代表UE编译。

隔离PostgreSQL17测试最终证据：`Saved/Validation/GamePlatformSession/Backend-42f99d203fda4f74b42b7d4347383f2f/backend.log`和`result.json`。真实Go race测试二进制执行Migrate、AtomicClaimAndCommit、CapacityAndIdentity、MigrationFenceAndExpiry、PersistencePrepare、PersistenceAfterRestart六项通过，实际重启测试数据库后继续验证持久化。仅使用本轮临时容器，清理完成；没有操作既有服务数据库。

早期两次SQL迁移因authorization标识语法失败，改成authorization_key后复跑通过；旧失败日志保留。数据库内核没有接入五服务公开路由、真实身份授权、UE连接或服务器注册，测试SQL夹具不是生产认证。

UE三目标、真实Online/Session准入、两个客户端、旅行、断线重连、多PIE、Cook与人工审查：未执行。不得把内核/数据库测试升级成完整Session插件验收。
