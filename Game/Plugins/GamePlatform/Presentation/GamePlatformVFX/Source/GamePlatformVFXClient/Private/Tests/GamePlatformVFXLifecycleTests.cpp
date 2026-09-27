#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXInstanceLifecycleTest,
    "GamePlatform.VFX.Lifecycle.InstanceRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXInstanceLifecycleTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXInstanceRegistry Registry;
    const FGamePlatformVFXHandle Handle = Registry.Reserve();

    TestTrue(TEXT("Reserved handle 有效"), Handle.IsValid());
    TestTrue(TEXT("Pending 句柄视为活动"), Registry.IsActive(Handle));
    TestTrue(TEXT("Stop 成功"), Registry.Stop(Handle));
    TestFalse(TEXT("Stop 后不再活动"), Registry.IsActive(Handle));
    return true;
}

#endif
