# AttributesAndVitalState（属性与生命状态）

所有正式战斗属性集统一继承 `UGamePlatformAttributeSet（游戏平台属性集基类）`，保持 AbilitySet（能力集合）软类约束、GAS 生命周期和跨插件公开类型边界一致。

- `UGamePlatformCombatAttributeSet（生命/护盾与Meta结算属性集）`：Health、MaxHealth、Shield、MaxShield，以及不复制的 IncomingDamage/IncomingHealing 元属性。前四项使用 RepNotify 复制；默认 MaxHealth=100、Health=100、MaxShield=100、Shield=0。
- `UGamePlatformOffenseAttributeSet（攻击属性集）`：AttackPower、AbilityPower、AttackSpeed、CriticalChance、CriticalDamage、ArmorPenetration、MagicPenetration。CriticalChance 约束为 0..1，其余当前只做非负有限值门禁；最终伤害公式仍由 Combat Execution/上层项目策略解释。
- `UGamePlatformDefenseAttributeSet（防御属性集）`：Armor、MagicResistance、DamageReduction；DamageReduction 约束为 0..1。
- `UGamePlatformControlAttributeSet（控制与韧性属性集）`：Tenacity、Poise、MaxPoise、PoiseRegen；Tenacity 约束为 0..1，Poise 始终限制在 0..MaxPoise。

上述默认值只是跨游戏安全基线，不代表《神兽联盟》最终平衡数值。装备与成长系统必须继续通过 GameplayEffect/AbilitySet 修改正式属性，不维护第二套 Attack/Defense 数值。

`UGamePlatformCombatAttributeSet::PostGameplayEffectExecute`读取 IncomingDamage/IncomingHealing 后立即清零元属性，并将实际结算交给 CombatComponent。死亡状态 `bDead`、AvatarGeneration、WorldContextGeneration 由 CombatComponent 复制；活动 Stun/Silence 通过 GAS GameplayEffect/Tag 复制。
