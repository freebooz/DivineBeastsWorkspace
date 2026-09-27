# ScoringObjectivesAndWinConditions（计分、目标与胜负）

竞技系统只消费可信Combat/Objective事件，并用EventId去重。Death更新K/D，Assist单独计数，Objective事件更新队伍目标分；Arena不重新计算Damage（伤害）。

当前第一版提供标准分数阈值与时间到期判定，同时保留 `IGamePlatformArenaScorePolicy（评分策略接口）`、`IGamePlatformArenaWinConditionPolicy（胜负策略接口）` 供后续数据驱动扩展。客户端不能SetScore/SetWinner。