# PersistenceAndReconciliation（持久化与对账）

跨会话任务真源位于既有 `PlayerDataService（玩家数据服务） + PostgreSQL`。UE `GamePlatformQuestServer`不直接访问数据库，而通过异步 `IGamePlatformQuestPersistencePort（任务持久化端口）`加载/接取/保存进度/完成/放弃；DBAServer 已提供基于异步 `FHttpModule（HTTP模块）`的 PlayerData 实现。真实 UE→Go 运行联调仍因 UE/Go/PostgreSQL 环境缺失而未执行。

普通 Objective（目标）事件先在 UE Dedicated Server（专用服务器）内存中即时推进，再按 `ProgressFlushDelaySeconds（进度刷新延迟）`聚合 `QuestId → EventIds[]`并批量写入绝对 Snapshot（快照）。接取、放弃、任务完成和迁服/登出前 Flush 属于高价值状态，要求立即持久化。

PostgreSQL 使用 `revision（修订号）`乐观并发：更新条件包含 `WHERE revision = expected_revision`，成功后 `revision++`；冲突返回 RevisionConflict，不覆盖较新状态。UE 冲突后进入 `bReconcileRequired（必须对账）`，先从 PlayerData 重新加载快照，再重放仍未持久化的完整 Pending QuestEvent（待持久化任务事件）；重载失败时不会继续拿旧 Revision 写入。

`player_quest_processed_event（已处理任务事件表）`以 game/player/quest_instance/event_id 为主键，使服务重启后同一任务实例仍能识别事件重放；一次可信事实可推进多个不同 Quest，因为幂等作用域包含 quest_instance_id。

QuestServer 对每个玩家只允许一个持久化请求在途；持久化期间到达的新事件进入有界 `DeferredEvents（延迟事件队列）`。迁服/登出时禁止新事件，但已经进入 Deferred/Pending 的事件会继续排空和持久化，队列清空后才移除玩家 Runtime。
