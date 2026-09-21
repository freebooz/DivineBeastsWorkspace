#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXLifecycleTest, "GamePlatform.VFX.Lifecycle.Smoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXLifecycleTest::RunTest(const FString&)
{
    AddInfo(TEXT("生命周期完整测试需要 PIE/World 测试夹具；此源码包保留入口，接入工程后补充。"));
    return true;
}
#endif
