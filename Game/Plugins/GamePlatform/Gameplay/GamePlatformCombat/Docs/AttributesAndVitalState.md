# AttributesAndVitalState（属性与生命状态）

**2026-10-09唯一正式基线：** 平台战斗只存在`UGamePlatformCombatAttributeSet（统一战斗属性集）`一个数值集，继承中立`UGamePlatformAttributeSet（平台GAS父类）`。攻击/防御收敛为DamageBonus（增伤）和DamageReduction（减伤）两项有符号修饰器；盾容量属于有期GameplayEffect（玩法效果）实例，不是GAS永久属性。项目气势另归DivineBeasts（神兽联盟项目层），MOBA层无新属性集。

- `UGamePlatformCombatAttributeSet（统一战斗属性集）`：Health（生命）、MaxHealth（最大生命）、DamageBonus（有符号增伤）、DamageReduction（有符号减伤），以及不复制的IncomingDamage/IncomingHealing（瞬时伤害/治疗）元属性；默认生命100/上限100，增伤和减伤为0。仅生命2项公开、增减伤2项OwnerOnly（仅拥有者）。
- `UGamePlatformOffenseAttributeSet（旧攻击属性集）`已退休，不保留同义数值或空反射类；原攻击/技能强度、暴击、穿透均不作为运行时属性。
- `UGamePlatformDefenseAttributeSet（旧防御属性集）`已退休。物理/法术共用DamageReduction（减伤）加减逻辑；数值允许正减免与负易伤，防御Buff/Debuff通过GAS修饰器作用于统一属性。
- **不设控制/韧性数值属性集**：已移除 `UGamePlatformControlAttributeSet（历史控制属性集）`、Tenacity（控制韧性）和失衡值系列。眩晕、沉默仍由正式GameplayEffect（玩法效果）与GameplayTag（玩法标签）表达；服务器直接使用经范围校验的技能控制持续时间，不应用数值抗性减免。

上述默认值只是跨游戏安全基线，不代表《神兽联盟》最终平衡数值。装备与成长系统必须继续通过 GameplayEffect/AbilitySet 修改正式属性，不维护第二套 Attack/Defense 数值。

当前全项目为2个具体属性集、8字段：生命2项对相关观察者复制，增减伤2项和气势2项OwnerOnly（仅拥有者），IncomingDamage/IncomingHealing（伤害/治疗元属性）2项不复制。盾由独立GAS有限时效果与服务器实例账本处理。最新依据见`GamePlatformAbilitySystem/Docs/GAS统一增减伤与护盾效果实施规范_20261009.md`（统一增减伤规范）。

`UGamePlatformCombatAttributeSet::PostGameplayEffectExecute`读取 IncomingDamage/IncomingHealing 后立即清零元属性，并将实际结算交给 CombatComponent。死亡状态 `bDead`、AvatarGeneration、WorldContextGeneration 由 CombatComponent 复制；活动 Stun/Silence 通过 GAS GameplayEffect/Tag 复制。
