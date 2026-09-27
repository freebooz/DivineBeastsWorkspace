#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformQuestStateMachine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformQuestStateMachineTest,
    "GamePlatform.Quest.StateMachine.Transitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformQuestStateMachineTest::RunTest(const FString& Parameters)
{
    EGamePlatformQuestState State = EGamePlatformQuestState::Available;

    TestTrue(
        TEXT("Available可进入Active"),
        FGamePlatformQuestStateMachine::TryTransition(
            State,
            EGamePlatformQuestState::Active));

    TestTrue(
        TEXT("Active可进入ObjectivesCompleted"),
        FGamePlatformQuestStateMachine::TryTransition(
            State,
            EGamePlatformQuestState::ObjectivesCompleted));

    TestTrue(
        TEXT("ObjectivesCompleted可进入CompletionPending"),
        FGamePlatformQuestStateMachine::TryTransition(
            State,
            EGamePlatformQuestState::CompletionPending));

    TestTrue(
        TEXT("CompletionPending可进入Completed"),
        FGamePlatformQuestStateMachine::TryTransition(
            State,
            EGamePlatformQuestState::Completed));

    TestFalse(
        TEXT("Completed不能回退Active"),
        FGamePlatformQuestStateMachine::TryTransition(
            State,
            EGamePlatformQuestState::Active));

    return true;
}
#endif
