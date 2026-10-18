// SSLM V6有期GAS增减伤：资产数值来自经签审的角色定义，不根据客户端或Niagara计算。
#include "Effects/DivineBeastsV6PassiveGameplayEffects.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "GameplayEffectTypes.h"

namespace
{
void BuildStackedPassiveEffect(UGameplayEffect& Effect,
    const FGameplayAttribute& Attribute, FName MagnitudeName,
    EGameplayEffectStackingType StackMode)
{
    Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
    // UE5.8的SetStackingType可能为Editor内部接口；CDO期间直接设置现有已开放字段。
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Effect.StackingType = StackMode;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    // CDO硬安全上限，正式资产MaxStacks由组件在施加前严格验证≤5；
    // 细分上限需在正式蓝图生成时逐资产验收，不能把此硬上限冒充策划MaxStacks。
    Effect.StackLimitCount = 5;
    Effect.StackDurationRefreshPolicy =
        EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

    FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = Attribute;
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat Caller;
    Caller.DataName = MagnitudeName;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Caller);
}
}

FName UDivineBeastsV6TigerFractureGameplayEffect::MagnitudeDataName()
{
    return FName(TEXT("SSLM.V6.Passive.Tiger.FractureReduction"));
}
UDivineBeastsV6TigerFractureGameplayEffect::UDivineBeastsV6TigerFractureGameplayEffect()
{
    // 同一虎对单一猎物最多累计5层，其他虎的负减伤效果独立叠加，不能从客户端增加层数。
    BuildStackedPassiveEffect(*this,
        UGamePlatformCombatAttributeSet::GetDamageReductionAttribute(),
        MagnitudeDataName(), EGameplayEffectStackingType::AggregateBySource);
}
FName UDivineBeastsV6BoarBattleWillGameplayEffect::MagnitudeDataName()
{
    return FName(TEXT("SSLM.V6.Passive.Boar.DamageBonus"));
}
UDivineBeastsV6BoarBattleWillGameplayEffect::UDivineBeastsV6BoarBattleWillGameplayEffect()
{
    BuildStackedPassiveEffect(*this,
        UGamePlatformCombatAttributeSet::GetDamageBonusAttribute(),
        MagnitudeDataName(), EGameplayEffectStackingType::AggregateByTarget);
}
