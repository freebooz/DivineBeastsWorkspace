# AttributesAndVitalState（属性与生命状态）

现行无韧性设计：平台战斗仅保留生命结算、攻击、防御三组具体数值属性集，均继承 `UGamePlatformAttributeSet（游戏平台中立属性集基类）`。项目气势单独归 DivineBeasts（神兽联盟项目层），MOBA通用层不再建立控制属性集。

- `UGamePlatformCombatAttributeSet（生命/护盾与Meta结算属性集）`：Health、MaxHealth、Shield、MaxShield，以及不复制的 IncomingDamage/IncomingHealing 元属性。前四项使用 RepNotify 复制；默认 MaxHealth=100、Health=100、MaxShield=100、Shield=0。
- `UGamePlatformOffenseAttributeSet（攻击属性集）`：AttackPower（攻击强度）、AbilityPower（技能强度）、CriticalChance（暴击概率）、CriticalDamage（暴击倍率）、ArmorPenetration（物理穿透）、MagicPenetration（法术穿透），共6项。CritChance取0..1，内部属性仅对拥有者复制。原AttackSpeed（攻速）预留属性已移除。
- `UGamePlatformDefenseAttributeSet（防御属性集）`：Armor（物理护甲）、MagicResistance（法术抗性）、DamageReduction（通用减伤），共3项且仅对拥有者复制；DamageReduction限制为0..1。
- **不设控制/韧性数值属性集**：已移除 `UGamePlatformControlAttributeSet（历史控制属性集）`、Tenacity（控制韧性）和失衡值系列。眩晕、沉默仍由正式GameplayEffect（玩法效果）与GameplayTag（玩法标签）表达；服务器直接使用经范围校验的技能控制持续时间，不应用数值抗性减免。

上述默认值只是跨游戏安全基线，不代表《神兽联盟》最终平衡数值。装备与成长系统必须继续通过 GameplayEffect/AbilitySet 修改正式属性，不维护第二套 Attack/Defense 数值。

当前全项目为4个具体属性集、17个属性字段：生命/护盾4项按Actor网络相关性复制，攻击/防御/气势11项仅向拥有者复制，IncomingDamage/IncomingHealing（瞬时伤害/治疗元属性）2项不复制。三层实施与迁移风险以 `GamePlatformAbilitySystem/Docs/GAS无韧性精简属性实施规范_20261009.md`（最新无韧性GAS规范）为准。

`UGamePlatformCombatAttributeSet::PostGameplayEffectExecute`读取 IncomingDamage/IncomingHealing 后立即清零元属性，并将实际结算交给 CombatComponent。死亡状态 `bDead`、AvatarGeneration、WorldContextGeneration 由 CombatComponent 复制；活动 Stun/Silence 通过 GAS GameplayEffect/Tag 复制。
