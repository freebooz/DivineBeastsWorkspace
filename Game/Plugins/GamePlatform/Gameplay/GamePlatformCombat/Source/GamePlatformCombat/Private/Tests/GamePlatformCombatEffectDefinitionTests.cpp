#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "Tags/GamePlatformAbilitySystemTags.h"
#include "Tags/GamePlatformCombatTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatControlEffectDefinitionTest,
    "GamePlatform.Combat.Control.EffectDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatControlEffectDefinitionTest::RunTest(const FString& Parameters)
{
    const UGamePlatformStunGameplayEffect* Stun =
        GetDefault<UGamePlatformStunGameplayEffect>();
    const UGamePlatformSilenceGameplayEffect* Silence =
        GetDefault<UGamePlatformSilenceGameplayEffect>();

    const FGameplayTagContainer& StunGranted =
        Stun->GetGrantedTags();
    TestTrue(
        TEXT("Stun授予控制标签"),
        StunGranted.HasTagExact(GamePlatformCombatTags::Control_Stun));

    const FGameplayTagContainer& StunBlocked =
        Stun->GetBlockedAbilityTags();
    TestTrue(
        TEXT("Stun阻止Ability.Active"),
        StunBlocked.HasTagExact(GamePlatformAbilitySystemTags::Ability_Active));

    const FGameplayTagContainer& SilenceGranted =
        Silence->GetGrantedTags();
    TestTrue(
        TEXT("Silence授予控制标签"),
        SilenceGranted.HasTagExact(GamePlatformCombatTags::Control_Silence));

    const FGameplayTagContainer& SilenceBlocked =
        Silence->GetBlockedAbilityTags();
    TestTrue(
        TEXT("Silence阻止Ability.Spell"),
        SilenceBlocked.HasTagExact(GamePlatformAbilitySystemTags::Ability_Spell));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatSlowEffectDefinitionTest,
    "GamePlatform.Combat.Control.SlowEffectDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatSlowEffectDefinitionTest::RunTest(const FString&)
{
    const UGamePlatformSlowGameplayEffect* Slow = GetDefault<UGamePlatformSlowGameplayEffect>();
    if (!TestNotNull(TEXT("原生平台减速玩法效果"), Slow)) return false;
    TestEqual(TEXT("减速必须是有限时长GE"), Slow->DurationPolicy, EGameplayEffectDurationType::HasDuration);
    TestTrue(TEXT("真实减速Tag来自GAS GE而非表现"), Slow->GetGrantedTags().HasTagExact(
        GamePlatformCombatTags::Control_Slow));
    TestEqual(TEXT("减速只有一项GAS加法速度修饰器"), Slow->Modifiers.Num(), 1);
    if (Slow->Modifiers.Num() != 1) return false;
    const FGameplayModifierInfo& Modifier = Slow->Modifiers[0];
    TestEqual(TEXT("修饰对象为平台速度属性"),
        Modifier.Attribute, UGamePlatformCombatAttributeSet::GetMoveSpeedMultiplierAttribute());
    TestEqual(TEXT("减速必须使用加法聚合而非永久覆盖"),
        Modifier.ModifierOp.GetValue(), EGameplayModOp::Additive);
    TestEqual(TEXT("动态减速值由受限服务器传入"),
        Modifier.ModifierMagnitude.GetMagnitudeCalculationType(),
        EGameplayEffectMagnitudeCalculation::SetByCaller);
    TestEqual(TEXT("动态参数身份不得为空"),
        Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag,
        GamePlatformCombatTags::Data_Control_SlowFraction);
    return true;
}

#endif
