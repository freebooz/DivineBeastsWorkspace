# Architecture（架构）

`GamePlatformCombat`位于 GameFoundation/Gameplay，只包含一个 Runtime 模块。服务端权威和客户端可见类型共用该模块，所有最终伤害/治疗/控制/死亡入口都先验证 `GetOwner()->HasAuthority()`。

直接依赖 `GamePlatformCore`、`GamePlatformAbilitySystem` 和 UE GameplayAbilities；不依赖 Character、InputClient、Online、Session、Loading、UI、VFX、MobaCommon、DivineBeasts 或 Go 后端。

主链：服务器可信 Ability/逻辑 → CombatSpec → Target/Generation 校验 → 受信任 GameplayEffectSpec → ExecutionCalculation → IncomingDamage/IncomingHealing → AttributeSet → CombatComponent → CombatResult → CombatEvent + GameplayCue。

当前已有 `GamePlatformPresentationCore`，但 Combat 没有增加该依赖；表现出口通过中立 CombatEvent 和 GameplayCue 保持独立。
