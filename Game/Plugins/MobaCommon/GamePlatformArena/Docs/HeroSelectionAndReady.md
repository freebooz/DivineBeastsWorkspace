# HeroSelectionAndReady（选人与准备）

客户端仅通过自己的 `AGamePlatformArenaPlayerController（竞技玩家控制器）` 请求HeroDefinitionId和Ready。服务器校验当前阶段、Roster归属、选择锁定以及 `IGamePlatformArenaHeroEligibilityProvider（英雄资格提供接口）`。

资格Provider未配置时按Fail Closed（失败关闭）拒绝选人。所有玩家完成选人后进入ReadyCheck，全部Ready后进入服务器倒计时。

第一版 `Arena.Selection.Standard（标准选人策略）` 允许不同玩家选择相同HeroDefinitionId（英雄定义编号），但单个玩家一旦锁定不可再次修改。HeroSelection（英雄选择）默认30秒、ReadyCheck（准备确认）默认15秒；超时策略明确为Abort（中止比赛），不会自动猜测英雄或替玩家Ready。