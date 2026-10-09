# QuestModel（任务模型）

任务使用稳定 `QuestId（任务编号）`和 `QuestInstanceId（任务实例编号）`，不使用显示名称作为身份。每个 Snapshot 包含 DefinitionVersion、State、Revision、Objectives、CompletionId、RewardSetId、RewardClaimId、RewardStatus、PeriodId、AcceptedAtUtc、CompletedAtUtc。

Objective 目前支持 Interaction（交互）、Combat（战斗）、Region（区域）和 Tutorial（教学）四类；聚合策略支持 Count（计数）、Sum（数值累加）和 SetComplete（直接完成）。Count/Sum 最终都 Clamp（钳制）到 RequiredValue。

Quest Runtime（任务运行时）按玩家维护 `QuestId → Snapshot`以及 `EventType → Relevant Objective Bindings`索引，收到事件时只扫描当前玩家该事件类型关联的 Active Objective，不扫描全服任务。

RepeatPolicy（重复策略）第一版真实支持 OneShot（一次性）和 RepeatableManual（手工重复接取）；Periodic（周期任务）只保留 Unsupported 类型，不伪装 Daily/Weekly。

2026-09-30：Snapshot新增显示SnapshotSequence；相同Revision只接纳更高序列，0序列仅保留旧状态变化兼容。服务器GetPlayerPersistenceError暴露失败，重放等待与持久化Revision分离，详见[本轮说明](DesignRemediation-2026-09-30.md)。
