# RepeatableAndTimedQuests（重复与限时任务）

`OneShot（一次性）`会通过 PlayerData 的 `player_quest_completion（玩家任务完成表）`检查历史完成，完成后再次 Accept 返回 AlreadyCompleted。并发接取还受到 PostgreSQL Active Quest 唯一索引保护。

`RepeatableManual（手工重复）`允许完成/放弃后创建新的 QuestInstanceId；历史完成由 completion ledger（完成记录）保留。UE 当前运行时只保留同 QuestId 的最新/当前 Snapshot。

`PeriodicUnsupported（周期任务未支持）`和 TimeWindow Periodic 只建立类型，Definition 校验会拒绝。没有使用客户端本地时间，也没有伪造 Daily/Weekly PeriodId。

如果后续实现周期任务，PeriodId 必须由可信服务端/后端 UTC 时间和明确周期边界生成，并处理时区/夏令时规则。
