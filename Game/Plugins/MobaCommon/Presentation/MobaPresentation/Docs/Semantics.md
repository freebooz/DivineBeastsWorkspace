# Semantics（语义）

第一版由 MobaPresentationRuntime（MOBA共享表现语义模块）拥有17个Native Gameplay Tags（原生玩法标签）。

核心语义包括 Moba.Combat.Hit、Critical、Heal、Shield.Hit、Control.Apply；Moba.Ability.Cast.Start/Release、Projectile.Spawn、Area.Warning；Moba.Status.Apply/Remove；Moba.Character.Death/Respawn；以及 Moba.Arena.Match.Start/End、Score.Changed、Objective.Completed。

这些标签表达“发生了什么、需要什么表现语义”，不表达具体资源路径或具体播放器。FiveCamp、Faction、Element、KingSeal及旧五行标签禁止恢复。
