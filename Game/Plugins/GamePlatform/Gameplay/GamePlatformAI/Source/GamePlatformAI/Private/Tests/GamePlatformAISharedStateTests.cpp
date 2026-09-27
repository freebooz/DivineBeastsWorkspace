#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Tags/GamePlatformAITags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAISharedStateConstructionTest,
    "GamePlatform.AI.SharedState.Construction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAISharedStateConstructionTest::RunTest(
    const FString& Parameters)
{
    UGamePlatformAIStateComponent* State =
        NewObject<UGamePlatformAIStateComponent>();
    UGamePlatformAITargetComponent* Target =
        NewObject<UGamePlatformAITargetComponent>();

    TestNotNull(TEXT("AIStateComponent可创建"), State);
    TestNotNull(TEXT("AITargetComponent可创建"), Target);
    TestEqual(
        TEXT("默认公开状态为Disabled"),
        State->GetSnapshot().PublicState,
        EGamePlatformAIPublicState::Disabled);

    TestTrue(
        TEXT("Patrol标签有效"),
        GamePlatformAITags::Movement_Patrol.GetTag().IsValid());
    TestTrue(
        TEXT("Investigate标签有效"),
        GamePlatformAITags::Movement_Investigate.GetTag().IsValid());
    TestTrue(
        TEXT("Attack标签有效"),
        GamePlatformAITags::Combat_Attack.GetTag().IsValid());

    return true;
}

#endif
