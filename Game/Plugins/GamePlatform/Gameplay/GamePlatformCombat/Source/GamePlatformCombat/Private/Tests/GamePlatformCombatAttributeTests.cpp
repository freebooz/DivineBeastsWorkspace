#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatAttributeDefaultsTest,
    "GamePlatform.Combat.Attributes.DefaultsAndClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatAttributeDefaultsTest::RunTest(const FString& Parameters)
{
    UGamePlatformCombatAttributeSet* Attributes =
        NewObject<UGamePlatformCombatAttributeSet>();

    TestEqual(TEXT("默认MaxHealth"), Attributes->GetMaxHealth(), 100.0f);
    TestEqual(TEXT("默认Health"), Attributes->GetHealth(), 100.0f);
    TestEqual(TEXT("默认Shield"), Attributes->GetShield(), 0.0f);

    float NegativeMaxHealth = -10.0f;
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetMaxHealthAttribute(),
        NegativeMaxHealth);
    TestEqual(TEXT("MaxHealth不得为负"), NegativeMaxHealth, 0.0f);

    const uint32 NaNBits = 0x7FC00000u;
    float NaNDamage = 0.0f;
    FMemory::Memcpy(&NaNDamage, &NaNBits, sizeof(float));
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetIncomingDamageAttribute(),
        NaNDamage);
    TestTrue(TEXT("NaN元属性被规范化"), FMath::IsFinite(NaNDamage));
    TestEqual(TEXT("NaN伤害归零"), NaNDamage, 0.0f);

    float NegativeHealing = -50.0f;
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetIncomingHealingAttribute(),
        NegativeHealing);
    TestEqual(TEXT("负治疗归零"), NegativeHealing, 0.0f);
    return true;
}

#endif
