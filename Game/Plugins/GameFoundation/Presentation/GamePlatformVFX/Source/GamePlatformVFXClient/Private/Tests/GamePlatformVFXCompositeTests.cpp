#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXCompositeTest, "GamePlatform.VFX.Composite.Smoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCompositeTest::RunTest(const FString&)
{
    AddInfo(TEXT("Composite 运行测试需要 World/TimerManager；接入工程后补充。"));
    return true;
}
#endif
