#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformCombatMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatShieldSettlementTest,
    "GamePlatform.Combat.Math.ShieldAndOverflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatShieldSettlementTest::RunTest(const FString& Parameters)
{
    const FGuid EventId = FGuid::NewGuid();
    const FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveDamage(
            EventId,
            80.0f,
            80.0f,
            50.0f,
            100.0f,
            false);

    TestEqual(TEXT("护盾吸收50"), Result.AppliedToShield, 50.0f);
    TestEqual(TEXT("溢出伤害30进入生命"), Result.AppliedToHealth, 30.0f);
    TestEqual(TEXT("护盾归零"), Result.RemainingShield, 0.0f);
    TestEqual(TEXT("生命剩70"), Result.RemainingHealth, 70.0f);
    TestFalse(TEXT("未死亡"), Result.bCausedDeath);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatBypassShieldTest,
    "GamePlatform.Combat.Math.BypassShield",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatBypassShieldTest::RunTest(const FString& Parameters)
{
    const FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveDamage(
            FGuid::NewGuid(),
            25.0f,
            25.0f,
            50.0f,
            100.0f,
            true);

    TestEqual(TEXT("绕盾不扣护盾"), Result.AppliedToShield, 0.0f);
    TestEqual(TEXT("绕盾直接扣生命"), Result.AppliedToHealth, 25.0f);
    TestEqual(TEXT("护盾仍为50"), Result.RemainingShield, 50.0f);
    TestEqual(TEXT("生命剩75"), Result.RemainingHealth, 75.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatOverkillTest,
    "GamePlatform.Combat.Math.OverkillClampsAtZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatOverkillTest::RunTest(const FString& Parameters)
{
    const FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveDamage(
            FGuid::NewGuid(),
            500.0f,
            500.0f,
            0.0f,
            100.0f,
            false);

    TestEqual(TEXT("实际只扣100生命"), Result.AppliedToHealth, 100.0f);
    TestEqual(TEXT("生命不能小于0"), Result.RemainingHealth, 0.0f);
    TestTrue(TEXT("造成死亡"), Result.bCausedDeath);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatHealingClampTest,
    "GamePlatform.Combat.Math.HealingClampsAtMax",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatHealingClampTest::RunTest(const FString& Parameters)
{
    const FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveHealing(
            FGuid::NewGuid(),
            80.0f,
            80.0f,
            70.0f,
            100.0f,
            15.0f);

    TestEqual(TEXT("实际治疗只有30"), Result.AppliedToHealth, 30.0f);
    TestEqual(TEXT("生命不超过上限"), Result.RemainingHealth, 100.0f);
    TestEqual(TEXT("治疗不改变护盾"), Result.RemainingShield, 15.0f);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatPhysicalFormulaTest,
    "GamePlatform.Combat.Math.Formula.PhysicalAndPenetration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatPhysicalFormulaTest::RunTest(const FString& Parameters)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.AttackPower = 100.0f;
    Input.AttackPowerCoefficient = 1.0f;
    Input.DamageType = EGamePlatformDamageType::Physical;
    Input.Armor = 100.0f;
    Input.DefenseMitigationConstant = 100.0f;

    FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("物理伤害Raw为200"), Result.RawDamage, 200.0f);
    TestEqual(TEXT("有效护甲为100"), Result.EffectiveDefense, 100.0f);
    TestEqual(TEXT("100护甲按默认常数减伤50%"), Result.FinalDamage, 100.0f);

    Input.ArmorPenetration = 50.0f;
    Result = FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("50穿透后有效护甲为50"), Result.EffectiveDefense, 50.0f);
    TestTrue(TEXT("穿透提高最终伤害"),
        FMath::IsNearlyEqual(Result.FinalDamage, 133.33333f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatMagicFormulaTest,
    "GamePlatform.Combat.Math.Formula.MagicAndReduction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatMagicFormulaTest::RunTest(const FString& Parameters)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.AbilityPower = 100.0f;
    Input.AbilityPowerCoefficient = 0.5f;
    Input.DamageType = EGamePlatformDamageType::Magic;
    Input.MagicResistance = 50.0f;
    Input.MagicPenetration = 25.0f;
    Input.DamageReduction = 0.2f;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("魔法Raw为150"), Result.RawDamage, 150.0f);
    TestEqual(TEXT("有效法抗为25"), Result.EffectiveDefense, 25.0f);
    TestTrue(TEXT("魔抗与通用减伤均参与结算"),
        FMath::IsNearlyEqual(Result.FinalDamage, 96.0f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatCriticalFormulaTest,
    "GamePlatform.Combat.Math.Formula.Critical",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatCriticalFormulaTest::RunTest(const FString& Parameters)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.bCanCritical = true;
    Input.CriticalChance = 1.0f;
    Input.CriticalDamage = 2.0f;
    Input.CriticalRoll = 0.5f;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestTrue(TEXT("100%暴击率必须暴击"), Result.bCritical);
    TestEqual(TEXT("2倍暴击伤害"), Result.FinalDamage, 200.0f);

    const FGuid EventId(1, 2, 3, 4);
    const float RollA = FGamePlatformCombatMath::MakeDeterministicUnitRoll(EventId);
    const float RollB = FGamePlatformCombatMath::MakeDeterministicUnitRoll(EventId);
    TestEqual(TEXT("同一EventId暴击掷值确定"), RollA, RollB);
    TestTrue(TEXT("确定性掷值必须在0..1"), RollA >= 0.0f && RollA < 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatTrueDamageFormulaTest,
    "GamePlatform.Combat.Math.Formula.TrueDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatTrueDamageFormulaTest::RunTest(const FString& Parameters)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 120.0f;
    Input.DamageType = EGamePlatformDamageType::TrueDamage;
    Input.Armor = 10000.0f;
    Input.MagicResistance = 10000.0f;
    Input.DamageReduction = 1.0f;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("真实伤害忽略护甲法抗与通用减伤"), Result.FinalDamage, 120.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatControlTenacityFormulaTest,
    "GamePlatform.Combat.Math.Control.Tenacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatControlTenacityFormulaTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("25%韧性将4秒控制缩短为3秒"),
        FGamePlatformCombatMath::CalculateControlDuration(4.0f, 0.25f),
        3.0f);
    TestEqual(TEXT("100%韧性完全抵抗控制"),
        FGamePlatformCombatMath::CalculateControlDuration(4.0f, 1.0f),
        0.0f);
    TestEqual(TEXT("负韧性按0处理"),
        FGamePlatformCombatMath::CalculateControlDuration(4.0f, -1.0f),
        4.0f);
    return true;
}

#endif
