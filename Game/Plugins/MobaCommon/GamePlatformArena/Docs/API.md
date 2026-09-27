# API（接口说明）

核心公共接口包括 `IGamePlatformArenaHeroEligibilityProvider（英雄资格提供接口）`、`IGamePlatformArenaGameplayLifecycleAdapter（玩法生命周期适配接口）`、`IGamePlatformArenaScorePolicy（评分策略接口）`、`IGamePlatformArenaWinConditionPolicy（胜负策略接口）`。

`AGamePlatformArenaGameMode（竞技游戏模式）` 提供服务器权威的Assignment应用、准入、选人、Ready、可信事件、弃权、重连、结束与结果确认。`FGamePlatformArenaServerBackendClient（竞技服务器后端客户端）` 只访问既有GameServerControlService（游戏服务器控制服务）内部接口。