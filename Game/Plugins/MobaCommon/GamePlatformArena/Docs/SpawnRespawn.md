# SpawnRespawn（出生与复活）

Arena只决定Which Spawn Group/When（使用哪个出生组与何时出生）。`IGamePlatformArenaGameplayLifecycleAdapter（玩法生命周期适配接口）` 将Spawn/Respawn请求交给现有Gameplay/Character权威生命周期实现。

死亡事实来自Combat（战斗）权威事件，Arena不直接SetHealth（设置生命值）或Revive（复活）。标准复活延迟当前为可配置默认值，策略ID来自ArenaModeDefinition。