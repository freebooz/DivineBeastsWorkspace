#include "Effects/GamePlatformCombatGameplayEffects.h"

#include "Execution/GamePlatformDamageExecutionCalculation.h"
#include "Execution/GamePlatformHealingExecutionCalculation.h"
#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/GamePlatformAbilitySystemTags.h"
#include "Tags/GamePlatformCombatTags.h"

namespace
{
void ConfigureGrantedTag(
    UGameplayEffect& Effect,
    const FGameplayTag& GrantedTag)
{
    UTargetTagsGameplayEffectComponent& Component =
        Effect.AddComponent<UTargetTagsGameplayEffectComponent>();

    FInheritedTagContainer Tags;
    Tags.Added.AddTag(GrantedTag);
    Component.SetAndApplyTargetTagChanges(Tags);
}

void ConfigureBlockedAbilityTag(
    UGameplayEffect& Effect,
    const FGameplayTag& BlockedAbilityTag)
{
    UBlockAbilityTagsGameplayEffectComponent& Component =
        Effect.AddComponent<UBlockAbilityTagsGameplayEffectComponent>();

    FInheritedTagContainer Tags;
    Tags.Added.AddTag(BlockedAbilityTag);
    Component.SetAndApplyBlockedAbilityTagChanges(Tags);
}

void ConfigureSingleStackDurationEffect(UGameplayEffect& Effect)
{
    Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
    // UE5.8 的 SetStackingType 仅 WITH_EDITOR 且未导出，跨插件 DLL 调用会产生链接错误。
    // 这里在 CDO 构造期直接初始化公开配置字段；字段在 UE5.8 仍可用，仅被标记为未来私有化。
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Effect.StackingType = EGameplayEffectStackingType::AggregateByTarget;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    Effect.StackLimitCount = 1;
    Effect.StackDurationRefreshPolicy =
        EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
}
}

UGamePlatformDamageGameplayEffect::UGamePlatformDamageGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass =
        UGamePlatformDamageExecutionCalculation::StaticClass();
    Executions.Add(Execution);
}

UGamePlatformHealingGameplayEffect::UGamePlatformHealingGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass =
        UGamePlatformHealingExecutionCalculation::StaticClass();
    Executions.Add(Execution);
}

UGamePlatformStunGameplayEffect::UGamePlatformStunGameplayEffect()
{
    ConfigureSingleStackDurationEffect(*this);
    ConfigureGrantedTag(*this, GamePlatformCombatTags::Control_Stun);
    ConfigureBlockedAbilityTag(*this, GamePlatformAbilitySystemTags::Ability_Active);
}

UGamePlatformSilenceGameplayEffect::UGamePlatformSilenceGameplayEffect()
{
    ConfigureSingleStackDurationEffect(*this);
    ConfigureGrantedTag(*this, GamePlatformCombatTags::Control_Silence);
    ConfigureBlockedAbilityTag(*this, GamePlatformAbilitySystemTags::Ability_Spell);
}
