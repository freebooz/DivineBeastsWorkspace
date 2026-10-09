#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "UObject/UnrealType.h"

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
    TestTrue(TEXT("唯一平台战斗数值属性集继承中立GAS基类"),
        UGamePlatformCombatAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));

    UGamePlatformCombatAttributeSet* Combat = NewObject<UGamePlatformCombatAttributeSet>();
    TestEqual(TEXT("初始伤害增强为0"), Combat->GetDamageBonus(), 0.0f);
    TestEqual(TEXT("初始伤害减免为0"), Combat->GetDamageReduction(), 0.0f);

    // 删除盾永久属性：临时盾只由GameplayEffect与组件效果实例负责。
    TestNull(TEXT("盾属性不再存在"),
        FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), FName(TEXT("Shield"))));
    TestNull(TEXT("最大盾属性不再存在"),
        FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), FName(TEXT("MaxShield"))));

    // Buff/Debuff可用有符号加减修饰器，不再提供攻击/防御/穿透/抗性等同义字段。
    float Weakness = -20.0f;
    Combat->PreAttributeChange(UGamePlatformCombatAttributeSet::GetDamageBonusAttribute(), Weakness);
    TestEqual(TEXT("削弱攻击的Debuff允许负增伤"), Weakness, -20.0f);
    float Vulnerability = -15.0f;
    Combat->PreAttributeChange(UGamePlatformCombatAttributeSet::GetDamageReductionAttribute(), Vulnerability);
    TestEqual(TEXT("降低减免的易伤Debuff允许负减免"), Vulnerability, -15.0f);

    for (const FName Retired : {
        FName(TEXT("AttackPower")), FName(TEXT("AbilityPower")),
        FName(TEXT("Penetration")), FName(TEXT("Resistance")),
        FName(TEXT("CriticalChance")), FName(TEXT("CriticalDamage"))
    })
    {
        TestNull(TEXT("旧攻击防御属性未迁入新的公共数值集"),
            FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), Retired));
    }

    return true;
}

#endif
