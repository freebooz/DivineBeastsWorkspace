#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformCombatMath.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"

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

    // 输入50是限时GE当前剩余吸收量，不是已废止的Shield AttributeSet字段。
    TestEqual(TEXT("有限时护盾效果吸收50"), Result.AppliedToShield, 50.0f);
    TestEqual(TEXT("溢出伤害30进入生命"), Result.AppliedToHealth, 30.0f);
    TestEqual(TEXT("护盾归零"), Result.RemainingShield, 0.0f);
    TestEqual(TEXT("生命剩70"), Result.RemainingHealth, 70.0f);
    TestFalse(TEXT("未死亡"), Result.bCausedDeath);
    return true;
}

/** 新盾效果只能以GAS有限时效果授予，不能作为常驻GAS数值重复复制。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatShieldEffectContractTest,
    "GamePlatform.Combat.Math.ShieldEffectContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatShieldEffectContractTest::RunTest(const FString&)
{
    const UGameplayEffect* Shield = GetDefault<UGamePlatformShieldGameplayEffect>();
    TestNotNull(TEXT("护盾限时GameplayEffect正式类型存在"), Shield);
    if (Shield)
    {
        TestEqual(TEXT("护盾效果必须有独立到期生命周期"),
            Shield->DurationPolicy, EGameplayEffectDurationType::HasDuration);
    }
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
            100.0f);

    TestEqual(TEXT("实际治疗只有30"), Result.AppliedToHealth, 30.0f);
    TestEqual(TEXT("生命不超过上限"), Result.RemainingHealth, 100.0f);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatPhysicalFormulaTest,
    "GamePlatform.Combat.Math.Formula.PhysicalAndPenetration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatPhysicalFormulaTest::RunTest(const FString&)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.DamageBonus = 20.0f;
    Input.DamageReduction = 15.0f;
    Input.DamageType = EGamePlatformDamageType::Physical;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("基础伤害加来源Buff后为120"), Result.RawDamage, 120.0f);
    TestEqual(TEXT("再扣目标减伤15后为105"), Result.FinalDamage, 105.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatMagicFormulaTest,
    "GamePlatform.Combat.Math.Formula.MagicAndReduction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatMagicFormulaTest::RunTest(const FString&)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.DamageBonus = 40.0f;
    Input.DamageReduction = 20.0f;
    Input.DamageType = EGamePlatformDamageType::Magic;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("法术增伤归一计算为140"), Result.RawDamage, 140.0f);
    TestEqual(TEXT("法术按同一减伤公式结算为120"), Result.FinalDamage, 120.0f);
    return true;
}

/**
 * 验证物理/法术共用增强伤害 - 减免伤害的纯加减公式。
 * 增伤/减伤可以是负数，分别用于削弱Buff和易伤Debuff，不能算出负伤害。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatUnifiedMitigationTest,
    "GamePlatform.Combat.Math.Formula.UnifiedMitigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatUnifiedMitigationTest::RunTest(const FString&)
{
    FGamePlatformDamageFormulaInput Input;
    Input.BaseDamage = 100.0f;
    Input.DamageBonus = 25.0f;
    Input.DamageReduction = 30.0f;
    Input.DamageType = EGamePlatformDamageType::Physical;
    const FGamePlatformDamageFormulaOutput Physical =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    Input.DamageType = EGamePlatformDamageType::Magic;
    const FGamePlatformDamageFormulaOutput Magic =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("物理与法术始终共用增伤减伤公式"), Physical.FinalDamage, Magic.FinalDamage);
    TestEqual(TEXT("100+25-30=95"), Physical.FinalDamage, 95.0f);

    Input.DamageReduction = -20.0f;
    TestEqual(TEXT("减伤Debuff降为负数时易伤增大到145"),
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input).FinalDamage, 145.0f);

    Input.DamageBonus = -200.0f;
    Input.DamageReduction = 0.0f;
    TestEqual(TEXT("基础伤害削弱不得降到0以下"),
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input).FinalDamage, 0.0f);
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
    Input.DamageBonus = 30.0f;
    Input.DamageReduction = 1000.0f;

    const FGamePlatformDamageFormulaOutput Result =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Input);
    TestEqual(TEXT("真实伤害保留来源增伤但忽略目标普通减伤"), Result.FinalDamage, 150.0f);
    return true;
}

#endif
