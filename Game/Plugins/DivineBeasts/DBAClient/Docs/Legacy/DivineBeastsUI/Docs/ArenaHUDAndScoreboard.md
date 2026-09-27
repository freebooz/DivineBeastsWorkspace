# ArenaHUDAndScoreboard（竞技HUD与计分板）

Arena HUD/Scoreboard只读 GamePlatformArena 复制事实：

- ArenaModeId
- MatchPhase
- RemainingPhaseSeconds（由服务器时间基准计算，不每秒RPC）
- TeamId/Score/ObjectiveScore/Revision
- Player CharacterId/Team/HeroDefinitionId
- Kills/Deaths/Assists/Score
- Ready状态
- ResultSummary

UDBAUICompositionSubsystem订阅GameState/PlayerState原生事件，事件驱动刷新，不用Tick轮询。

WinningTeamId和EndReason直接读取ResultSummary，UI不计算Winner（胜者）、Rating（评级）或Reward（奖励）。

PlayerId/CharacterId是内部身份；正式Widget不得直接显示UUID技术值。当前没有PlayerDisplayName公开投影，因此实际记分板姓名展示仍需后续Owner数据源。
