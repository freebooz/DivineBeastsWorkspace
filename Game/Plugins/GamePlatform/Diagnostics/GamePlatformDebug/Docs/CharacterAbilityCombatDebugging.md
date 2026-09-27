# CharacterAbilityCombatDebugging（角色/技能/战斗调试）

Character Provider（角色状态提供者）读取 Actor（对象）名称/类型、Authority（权威）、Location/Velocity（位置/速度）、LifecycleState（生命周期）和 CharacterMovement（角色移动）模式。

CharacterId（角色编号）、HeroDefinitionId（英雄定义编号）、Equipment（装备）与 Progression Level（成长等级）在当前插件未提供稳定只读接口或涉及 PlayerServices（玩家业务服务）边界时返回 N/A（不可用）。

Ability Provider（技能状态提供者）通过 UAbilitySystemComponent（技能系统组件）输出 ASC Owner/Avatar（拥有者/化身）、可激活技能数量、GameplayTags（玩法标签）、Attribute（属性）数量和 Active Gameplay Effects（活动玩法效果）数量。首版不每帧展开全部效果或冷却明细。

Combat Provider（战斗状态提供者）输出 Health/MaxHealth（生命/最大生命）、Shield/MaxShield（护盾/最大护盾）、Dead（死亡）和 AvatarGeneration（化身代次）。插件不存在 SetHealth/Revive（设置生命/复活）命令。