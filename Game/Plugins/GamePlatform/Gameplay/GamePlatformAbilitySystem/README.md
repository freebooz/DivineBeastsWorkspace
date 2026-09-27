# GamePlatformAbilitySystem（游戏平台技能系统插件）

跨游戏GAS（Gameplay Ability System，游戏技能系统）基础层：提供 `UGamePlatformAbilitySystemComponent（平台ASC基类）`、GameplayAbilities/GameplayTags/GameplayTasks依赖和中立Ability标签。

本轮增加Owner/Avatar ActorInfo统一绑定、AvatarGeneration代次与变更事件，重生/换Pawn可明确使旧Avatar上下文失效；空Owner/Avatar绑定Fail Closed。具体Ability Grant、输入映射、Cooldown、Cost和项目技能仍由后续AbilitySet/项目层扩展，不反向依赖Combat。
