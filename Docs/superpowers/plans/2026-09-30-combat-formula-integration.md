# 《神兽联盟》通用战斗属性公式接入执行计划

版本：1.0
日期：2026-09-30
工作空间：DivineBeastsWorkspace（神兽联盟工作空间）

## 1. 目标

在上一阶段核心 AttributeSet（属性集）和 Momentum（气势）已经落地的基础上，把平台攻击、防御和控制属性真正接入权威战斗结算，同时保持现有插件职责：

1. `GamePlatformCombat（游戏平台战斗插件）`负责跨游戏通用伤害类型、属性缩放、暴击、防御减伤和 Tenacity（韧性）控制时长修正。
2. `FGamePlatformCombatSpec（战斗规格）`继续由服务器可信 Ability/逻辑构造，不开放客户端通用伤害 RPC。
3. `GamePlatformEquipment（装备插件）`继续通过 GameplayEffect/AbilitySet 修改正式 GAS 属性，不建立第二套 Attack/Defense 数值。
4. `GamePlatformProgression（成长插件）`按现行规范保持成长快照/展示职责，不直接依赖 Combat 或写 GAS；需要把成长映射到战斗数值时，由上层 Gameplay 组合通过 AbilitySet/GameplayEffect 完成。
5. 不恢复旧 Element（五行）、克制、破元、共鸣。

## 2. 伤害公式

基础公式：

`Raw = BaseDamage + AttackPower * AttackPowerCoefficient + AbilityPower * AbilityPowerCoefficient`

若允许暴击且服务器确定性掷值小于 CriticalChance：

`Raw *= max(1, CriticalDamage)`

类型减伤：

- Physical（物理）：`EffectiveArmor=max(0, Armor-ArmorPenetration)`。
- Magic（魔法）：`EffectiveResistance=max(0, MagicResistance-MagicPenetration)`。
- Untyped（无类型）：不走 Armor/MagicResistance，保留通用 DamageReduction。
- TrueDamage（真实伤害）：忽略 Armor/MagicResistance 与 DamageReduction。

物理/魔法防御系数：

`Mitigation = DefenseConstant / (DefenseConstant + EffectiveDefense)`

默认 `DefenseConstant=100`，可通过 GamePlatformCombatSettings（战斗设置）调整。

非 TrueDamage 最后再应用：

`Final *= (1 - DamageReduction)`

最终仍受 `MaxDamageMagnitude（单次最大伤害）` 安全上限约束。

## 3. 控制公式

`FinalDuration = RequestedDuration * (1 - Tenacity)`

Tenacity 限制为 0..1；完全抵抗时不创建控制 GameplayEffect，返回 ControlResisted（控制已抵抗）结果标签。

## 4. 兼容策略

- 旧调用只填写 `Magnitude` 时，系数默认为 0、DamageType 默认为 Untyped、bCanCritical 默认为 false，因此旧伤害路径数值保持兼容。
- 新技能按 Definition/Ability 配置 AttackPowerCoefficient、AbilityPowerCoefficient、DamageType、bCanCritical，不硬编码技能名称。
- 服务器根据 EventId 生成确定性 CriticalRoll（暴击掷值），同一事件重放得到相同暴击结果。

## 5. 验证

- 增加纯函数公式测试：物理护甲、穿透、魔法抗性、真实伤害、暴击、通用减伤、确定性掷值。
- 增加控制 Tenacity 测试/编译验证。
- 运行项目头引用门禁与 UE5.8 定向模块编译。
