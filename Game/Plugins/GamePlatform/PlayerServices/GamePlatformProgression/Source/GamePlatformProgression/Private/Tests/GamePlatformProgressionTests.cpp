#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/GamePlatformProgressionTrackDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformProgressionCurveTest,
    "GamePlatform.Progression.Curve",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformProgressionCurveTest::RunTest(const FString&)
{
    UGamePlatformProgressionTrackDefinition* Definition =
        NewObject<UGamePlatformProgressionTrackDefinition>();

    Definition->ProgressionTrackId = TEXT("Character.Level");
    Definition->SubjectType =
        EGamePlatformProgressionSubjectType::Character;
    Definition->CurveVersion = 1;
    Definition->MinLevel = 1;
    Definition->MaxLevel = 4;
    Definition->CumulativeXPThresholds = {0, 100, 300, 600};

    FString Reason;
    TestTrue(TEXT("Curve有效"), Definition->Validate(Reason));
    TestEqual(TEXT("0XP为1级"), Definition->CalculateLevel(0), 1);
    TestEqual(TEXT("100XP为2级"), Definition->CalculateLevel(100), 2);
    TestEqual(TEXT("599XP为3级"), Definition->CalculateLevel(599), 3);
    TestEqual(TEXT("超过上限仍为4级"), Definition->CalculateLevel(9999), 4);

    Definition->CumulativeXPThresholds = {0, 100, 90, 600};
    TestFalse(TEXT("非单调Curve无效"), Definition->Validate(Reason));

    return true;
}

#endif
