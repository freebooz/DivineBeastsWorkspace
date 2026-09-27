# CombatGameplayIntegration（战斗与玩法集成）

Arena消费GamePlatformCombat（游戏平台战斗）产生的可信Death/Assist等事实，并用EventId保证幂等；Arena不重新计算Damage。

出生和复活通过 `IGamePlatformArenaGameplayLifecycleAdapter（玩法生命周期适配接口）` 进入既有Gameplay/Character服务器能力，竞技插件不直接修改Health（生命值）。