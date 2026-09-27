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

#endif
