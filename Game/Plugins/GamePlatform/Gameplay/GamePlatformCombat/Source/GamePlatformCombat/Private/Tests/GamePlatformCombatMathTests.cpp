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

#endif
