# Semantics（语义）

MobaPresentationRuntime（MOBA共享表现语义模块）统一注册17个Native Gameplay Tags（原生玩法标签）路径，其中16个属于当前有效表现语义，1个仅保留历史资产兼容。标签由Runtime原生源码注册，不由测试或项目标签配置临时补入。

当前16个有效语义包括 Moba.Combat.Hit、Heal、Shield.Hit、Control.Apply；Moba.Ability.Cast.Start/Release、Projectile.Spawn、Area.Warning；Moba.Status.Apply/Remove；Moba.Character.Death/Respawn；以及 Moba.Arena.Match.Start/End、Score.Changed、Objective.Completed。

Moba.Combat.Critical（历史暴击标签）保留已发布的Native路径，供旧资产加载与身份兼容；不进入FMobaPresentationSemanticRegistry（有效表现语义注册表），不再产生或提交暴击表现。原生标签仍存在不代表对应玩法或表现输出恢复，不能为满足旧数量断言重新登记为有效语义。

这些标签表达“发生了什么、需要什么表现语义”，不表达具体资源路径或具体播放器。FiveCamp、Faction、Element、KingSeal及旧五行标签禁止恢复。

Moba.Presentation.Runtime.SemanticRegistry（语义注册表回归）检查真实注册表有效、16个现行语义逐项可查，以及历史Critical原生身份有效但未进入有效表；不新增测试标签或伪造资源成功。回归源码与中文说明保持这一合同，实际编译与UE运行结果另行留证。
