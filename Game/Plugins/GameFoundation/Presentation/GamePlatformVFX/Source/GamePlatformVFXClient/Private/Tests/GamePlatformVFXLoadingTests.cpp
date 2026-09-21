#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXLoadingTest, "GamePlatform.VFX.Loading.Smoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXLoadingTest::RunTest(const FString&)
{
    AddInfo(TEXT("异步加载测试需要真实 PrimaryAsset 配置；接入工程后补充。"));
    return true;
}
#endif
