# ArenaIntegration（竞技集成）

AGamePlatformArenaGameState（竞技游戏状态）新增只读Native Delegate出口，并通过RepNotify（复制通知）广播MatchPhase、TeamStates、ObjectiveStates、ResultSummary变化；AGamePlatformArenaPlayerState（竞技玩家状态）通过StatsRevision复制通知广播统计变化。

MobaPresentationClient只读取这些事实，映射 Match.Start、Match.End、Score.Changed、Objective.Completed，不调用AddScore、SetWinner、ChangePhase或SubmitResult。

Late Join（晚加入）只建立当前Phase基线，不重播历史Match.Start/End；当前Score属于持续状态，可恢复为Persistent Request（持续请求）。
