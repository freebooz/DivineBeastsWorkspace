#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Targeting/GamePlatformAITargetSelectionRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAITargetSelectionTest,
    "GamePlatform.AI.Targeting.DeterministicOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAITargetSelectionTest::RunTest(const FString& Parameters)
{
    FGamePlatformAITargetSortKey Current;
    Current.bEligible = true;
    Current.bVisible = false;
    Current.DistanceSquared = 100.0f;
    Current.LastSensedTime = 10.0;
    Current.EntityId =
        FGuid(0, 0, 0, 2);

    FGamePlatformAITargetSortKey Candidate = Current;
    Candidate.bVisible = true;
    Candidate.DistanceSquared = 400.0f;

    TestTrue(
        TEXT("启用可见优先时可见目标优先"),
        FGamePlatformAITargetSelectionRules::IsBetter(
            Candidate,
            Current,
            true));

    Candidate = Current;
    Candidate.DistanceSquared = 25.0f;
    TestTrue(
        TEXT("同可见性距离更近优先"),
        FGamePlatformAITargetSelectionRules::IsBetter(
            Candidate,
            Current,
            true));

    Candidate = Current;
    Candidate.EntityId =
        FGuid(0, 0, 0, 1);
    TestTrue(
        TEXT("完全相同时稳定EntityId作为TieBreak"),
        FGamePlatformAITargetSelectionRules::IsBetter(
            Candidate,
            Current,
            true));

    return true;
}

#endif
