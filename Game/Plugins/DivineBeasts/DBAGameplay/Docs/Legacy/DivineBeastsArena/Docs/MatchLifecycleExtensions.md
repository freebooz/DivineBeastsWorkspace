# MatchLifecycleExtensions（比赛生命周期扩展）

项目不创建第二套Phase（阶段）状态机。

继续使用平台：
HeroSelection → ReadyCheck → Countdown → InProgress → Ending → ResultPending。

DivineBeastsArenaServer只在Assignment应用前执行项目配置/Revision/Hero资格门禁，并向平台GameMode注入HeroEligibilityProvider。

当前GameplayLifecycleAdapter缺失，所以即使未来Production规则补齐，服务器仍会在进入真实Spawn生命周期前Fail Closed，直到平台统一Spawn/Respawn适配完成。
