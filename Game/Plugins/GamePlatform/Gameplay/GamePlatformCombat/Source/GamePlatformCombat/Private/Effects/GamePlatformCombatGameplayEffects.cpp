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
    UTargetTagsGameplayEffectComponent& Component,
    const FGameplayTag& GrantedTag)
{
    FInheritedTagContainer Tags;
    Tags.Added.AddTag(GrantedTag);
    Component.SetAndApplyTargetTagChanges(Tags);
}

void ConfigureBlockedAbilityTag(
    UBlockAbilityTagsGameplayEffectComponent& Component,
    const FGameplayTag& BlockedAbilityTag)
{
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

UGamePlatformStunGameplayEffect::UGamePlatformStunGameplayEffect(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigureSingleStackDurationEffect(*this);

    // GameplayEffect::AddComponent 在 UE 5.8 构造期会使用空名称 NewObject，触发类默认对象创建致命错误。
    // 这里使用具名默认子对象，保证 CDO、派生蓝图和热重载过程中的对象身份均保持稳定。
    UTargetTagsGameplayEffectComponent* GrantedTagsComponent =
        ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
            this,
            TEXT("GrantedControlTags"));
    UBlockAbilityTagsGameplayEffectComponent* BlockedAbilityTagsComponent =
        ObjectInitializer.CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(
            this,
            TEXT("BlockedAbilityTags"));
    GEComponents.Add(GrantedTagsComponent);
    GEComponents.Add(BlockedAbilityTagsComponent);

    ConfigureGrantedTag(*GrantedTagsComponent, GamePlatformCombatTags::Control_Stun);
    ConfigureBlockedAbilityTag(
        *BlockedAbilityTagsComponent,
        GamePlatformAbilitySystemTags::Ability_Active);
}

UGamePlatformSilenceGameplayEffect::UGamePlatformSilenceGameplayEffect(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigureSingleStackDurationEffect(*this);

    // 与眩晕效果保持同一默认子对象契约，避免构造期动态对象名称不确定。
    UTargetTagsGameplayEffectComponent* GrantedTagsComponent =
        ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
            this,
            TEXT("GrantedControlTags"));
    UBlockAbilityTagsGameplayEffectComponent* BlockedAbilityTagsComponent =
        ObjectInitializer.CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(
            this,
            TEXT("BlockedAbilityTags"));
    GEComponents.Add(GrantedTagsComponent);
    GEComponents.Add(BlockedAbilityTagsComponent);

    ConfigureGrantedTag(*GrantedTagsComponent, GamePlatformCombatTags::Control_Silence);
    ConfigureBlockedAbilityTag(
        *BlockedAbilityTagsComponent,
        GamePlatformAbilitySystemTags::Ability_Spell);
}
