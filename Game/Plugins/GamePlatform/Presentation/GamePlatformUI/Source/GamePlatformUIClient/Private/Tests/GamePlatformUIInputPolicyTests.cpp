#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "CommonInputModeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIInputPolicyTest,
    "GamePlatform.UI.Input.PolicyModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIInputPolicyTest::RunTest(const FString& Parameters)
{
    const FUIInputConfig GameOnly =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameOnly);
    const FUIInputConfig UIOnly =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::UIOnly);
    const FUIInputConfig Both =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameAndUI);

    TestEqual(TEXT("GameOnly"), GameOnly.GetInputMode(), ECommonInputMode::Game);
    TestEqual(TEXT("UIOnly"), UIOnly.GetInputMode(), ECommonInputMode::Menu);
    TestEqual(TEXT("GameAndUI"), Both.GetInputMode(), ECommonInputMode::All);
    TestTrue(TEXT("UIOnly忽略移动"), UIOnly.bIgnoreMoveInput);
    TestTrue(TEXT("UIOnly忽略视角"), UIOnly.bIgnoreLookInput);
    TestFalse(TEXT("无World不能暂停"), UGamePlatformUIInputPolicy::CanPauseWorld(
        nullptr,
        EGamePlatformUIPausePolicy::StandaloneOnly));
    return true;
}

#endif
