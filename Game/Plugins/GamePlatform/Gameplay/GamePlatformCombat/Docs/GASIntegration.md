# GASIntegration（GAS集成）

Combat 复用 UE GameplayAbilities 和 `GamePlatformAbilitySystem`，没有复制 Ability Grant、PredictionKey、AbilityInput、Cooldown 或 Cost。

由于前置 AbilitySystem 原先完全为空，本轮仅新增最小 `UGamePlatformAbilitySystemComponent`（默认复制、Full GameplayEffect Replication）以及 `Ability.Active`/`Ability.Spell`分类标签。完整 AbilitySystem 仍未生产化。

CombatComponent 在 BeginPlay 查找平台 ASC；服务器若不存在 CombatAttributeSet，则通过 ASC `AddSet`加入正式属性集。

Damage/Healing 的唯一主路径是 GameplayEffect Execution → Incoming meta attribute → CombatComponent；没有同时直接 SetHealth 再 Apply GE 的双重扣血路径。
