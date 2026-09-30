#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Attributes/GamePlatformControlAttributeSet.h"
#include "Attributes/GamePlatformDefenseAttributeSet.h"
#include "Attributes/GamePlatformOffenseAttributeSet.h"

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatAttributeFamiliesTest,
    "GamePlatform.Combat.Attributes.Families",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatAttributeFamiliesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("生命/Meta属性集必须继承平台AttributeSet"),
        UGamePlatformCombatAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));
    TestTrue(TEXT("攻击属性集必须继承平台AttributeSet"),
        UGamePlatformOffenseAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));
    TestTrue(TEXT("防御属性集必须继承平台AttributeSet"),
        UGamePlatformDefenseAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));
    TestTrue(TEXT("控制属性集必须继承平台AttributeSet"),
        UGamePlatformControlAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));

    UGamePlatformOffenseAttributeSet* Offense = NewObject<UGamePlatformOffenseAttributeSet>();
    UGamePlatformDefenseAttributeSet* Defense = NewObject<UGamePlatformDefenseAttributeSet>();
    UGamePlatformControlAttributeSet* Control = NewObject<UGamePlatformControlAttributeSet>();
    TestEqual(TEXT("默认AttackSpeed"), Offense->GetAttackSpeed(), 1.0f);
    TestEqual(TEXT("默认CriticalDamage"), Offense->GetCriticalDamage(), 1.5f);
    TestEqual(TEXT("默认MaxPoise"), Control->GetMaxPoise(), 100.0f);
    TestEqual(TEXT("默认Poise"), Control->GetPoise(), 100.0f);

    float CriticalChance = 2.0f;
    Offense->PreAttributeChange(UGamePlatformOffenseAttributeSet::GetCriticalChanceAttribute(), CriticalChance);
    TestEqual(TEXT("CriticalChance限制在0..1"), CriticalChance, 1.0f);

    float DamageReduction = 2.0f;
    Defense->PreAttributeChange(UGamePlatformDefenseAttributeSet::GetDamageReductionAttribute(), DamageReduction);
    TestEqual(TEXT("DamageReduction限制在0..1"), DamageReduction, 1.0f);

    float Tenacity = -1.0f;
    Control->PreAttributeChange(UGamePlatformControlAttributeSet::GetTenacityAttribute(), Tenacity);
    TestEqual(TEXT("Tenacity限制在0..1"), Tenacity, 0.0f);
    return true;
}

#endif
