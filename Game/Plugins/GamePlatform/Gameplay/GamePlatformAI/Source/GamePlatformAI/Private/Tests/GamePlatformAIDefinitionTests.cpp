#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/GamePlatformAIDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAIDefinitionValidationTest,
    "GamePlatform.AI.Definition.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAIDefinitionValidationTest::RunTest(const FString& Parameters)
{
    UGamePlatformAIDefinition* Definition =
        NewObject<UGamePlatformAIDefinition>();

    FText Reason;
    TestFalse(
        TEXT("空DefinitionId必须拒绝"),
        Definition->ValidateDefinition(Reason));

    Definition->AIDefinitionId = TEXT("AI.Foundation");
    Definition->BrainType = EGamePlatformAIBrainType::BehaviorTree;
    Definition->BehaviorTreeAsset =
        FSoftObjectPath(TEXT("/Game/AI/BT_Test.BT_Test"));
    Definition->BlackboardAsset =
        FSoftObjectPath(TEXT("/Game/AI/BB_Test.BB_Test"));

    TestTrue(
        TEXT("结构完整BehaviorTree定义可通过静态验证"),
        Definition->ValidateDefinition(Reason));

    Definition->PerceptionProfile.LoseSightRadius =
        Definition->PerceptionProfile.SightRadius - 1.0f;

    TestFalse(
        TEXT("LoseSightRadius不得小于SightRadius"),
        Definition->ValidateDefinition(Reason));


    Definition->PerceptionProfile.LoseSightRadius =
        Definition->PerceptionProfile.SightRadius + 400.0f;
    Definition->PreferredRange =
        Definition->AttackRange + 1.0f;

    TestFalse(
        TEXT("PreferredRange不得大于AttackRange"),
        Definition->ValidateDefinition(Reason));

    Definition->PreferredRange =
        Definition->AttackRange * 0.75f;
    Definition->PerceptionProfile.PeripheralVisionHalfAngleDegrees =
        181.0f;

    TestFalse(
        TEXT("PeripheralVision不得超过180度"),
        Definition->ValidateDefinition(Reason));

    Definition->PerceptionProfile.PeripheralVisionHalfAngleDegrees =
        65.0f;
    Definition->PerceptionProfile.bEnableHearing = true;
    Definition->PerceptionProfile.HearingRange = 0.0f;

    TestFalse(
        TEXT("启用Hearing时Range必须大于0"),
        Definition->ValidateDefinition(Reason));

    return true;
}

#endif
