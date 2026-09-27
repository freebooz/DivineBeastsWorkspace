# AttributesAndVitalState（属性与生命状态）

`UGamePlatformCombatAttributeSet`包含 Health、MaxHealth、Shield、MaxShield，以及不复制的 IncomingDamage/IncomingHealing 元属性。前四项使用 RepNotify 复制。

默认值：MaxHealth=100、Health=100、MaxShield=100、Shield=0；这只是平台安全默认，不代表《神兽联盟》最终数值设计。

`PreAttributeChange`限制有限数值和合法范围；Incoming 元属性不允许负数。`PostGameplayEffectExecute`读取后立即清零元属性，并将实际结算交给 CombatComponent。

死亡状态 `bDead`、AvatarGeneration、WorldContextGeneration 由 CombatComponent 复制；活动 Stun/Silence 通过 GAS GameplayEffect/Tag 复制。
