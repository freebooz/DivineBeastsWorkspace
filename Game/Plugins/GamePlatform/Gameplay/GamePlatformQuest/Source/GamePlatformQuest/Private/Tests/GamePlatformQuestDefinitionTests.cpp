#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformQuestDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformQuestDefinitionTest,
    "GamePlatform.Quest.Definition.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformQuestDefinitionTest::RunTest(const FString& Parameters)
{
    UGamePlatformQuestDefinition* Definition =
        NewObject<UGamePlatformQuestDefinition>();

    FText Reason;
    TestFalse(TEXT("空Definition非法"), Definition->ValidateDefinition(Reason));

    Definition->QuestId = TEXT("Quest.Foundation.Tutorial");

    FGamePlatformQuestObjectiveDefinition Objective;
    Objective.ObjectiveId = TEXT("EnterArea");
    Objective.EventType = TEXT("World.Region.Entered");
    Objective.ObjectiveType = EGamePlatformQuestObjectiveType::Region;
    Objective.RequiredRegionId = TEXT("Foundation.TrainingArea");
    Definition->Objectives.Add(Objective);

    TestTrue(TEXT("基础任务定义合法"), Definition->ValidateDefinition(Reason));

    Definition->Objectives.Add(Objective);
    TestFalse(TEXT("重复ObjectiveId拒绝"), Definition->ValidateDefinition(Reason));
    return true;
}
#endif
