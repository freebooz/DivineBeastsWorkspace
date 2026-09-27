# DamageAndHealing（伤害与治疗）

Damage/Healing 使用 C++ Instant GameplayEffect + `UGameplayEffectExecutionCalculation`。运行值使用受信任 Spec 的 SetByCaller Tag：`Combat.Data.Damage` / `Combat.Data.Healing`。

Execution 拒绝 NaN/Infinity、零或负值，并按 CombatSettings 安全上限 Clamp，随后仅写 IncomingDamage/IncomingHealing，不直接访问数据库、UI 或比赛计分。

Damage 最终由 CombatComponent 结算 Shield/Health；Healing 只补 Health，实际治疗量为 `min(MaxHealth-Health, FinalHealing)`，不会用负 Damage 模拟治疗。

普通 Healing 对 Dead 目标在 Spec 校验阶段返回 TargetDead，不承担 Revive（复苏）。
