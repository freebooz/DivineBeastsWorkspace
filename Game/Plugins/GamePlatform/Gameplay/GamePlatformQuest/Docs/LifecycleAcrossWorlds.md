# LifecycleAcrossWorlds（跨世界生命周期）

每个 UWorld（世界）拥有独立 `UGamePlatformQuestServerSubsystem`。Player Runtime 使用可信 PlayerId + PlayerRuntimeId 标识当前服务器生命周期，收到 Event 时必须匹配 RuntimeId；旧服务器迟到事件不能直接冒充新生命周期。

迁服/登出前可调用 `FlushPlayerProgressNow（立即刷新玩家任务进度）`。异步模型不会在游戏线程等待网络：它发起剩余写入，并在仍有请求/Deferred/Pending/Reconcile 时返回 `PersistenceOutcomeUnknown（持久化结果未知）`。`UnregisterPlayer（注销玩家任务运行时）`只禁止新事件，已经进入队列的可信事件会继续排空并持久化，完成后才移除 Runtime。

新服务器应从 PlayerData `GetQuestSnapshot`重新加载并 Reconcile Revision，而不是使用旧客户端缓存覆盖后端。Quest 不调用 ClientTravel，也不决定 OpenWorld/Village/MainArena 实例选择。

Session 跨服 Hook（会话迁移钩子）当前尚未接到 Quest Flush/RegisterPlayer，真实 OpenWorld→Village/MainArena 恢复仍为未执行。
