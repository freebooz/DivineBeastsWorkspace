#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/GamePlatformNavigationAgentProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformNavigationAgentProfileTest,
    "GamePlatform.Navigation.AgentProfile.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformNavigationAgentProfileTest::RunTest(const FString& Parameters)
{
    UGamePlatformNavigationAgentProfile* Profile =
        NewObject<UGamePlatformNavigationAgentProfile>();

    FText Reason;
    TestFalse(TEXT("空ProfileId拒绝"), Profile->ValidateProfile(Reason));

    Profile->ProfileId = TEXT("Navigation.Agent.Foundation");
    TestTrue(TEXT("默认标准Agent合法"), Profile->ValidateProfile(Reason));

    Profile->InvokerPolicy =
        EGamePlatformNavigationInvokerPolicy::RegisterWhenActive;
    Profile->TileGenerationRadius = 5000.0f;
    Profile->TileRemovalRadius = 1000.0f;
    TestFalse(TEXT("Invoker移除半径不得小于生成半径"), Profile->ValidateProfile(Reason));

    return true;
}
#endif
