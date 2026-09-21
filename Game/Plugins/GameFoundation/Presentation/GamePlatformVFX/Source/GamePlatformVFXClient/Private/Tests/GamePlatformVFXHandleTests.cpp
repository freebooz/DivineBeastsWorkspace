#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformVFXHandle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXHandleTest, "GamePlatform.VFX.Handle.Smoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXHandleTest::RunTest(const FString&)
{
    FGamePlatformVFXHandle H; H.InstanceId = FGuid::NewGuid(); H.Generation = 1; TestTrue(TEXT("Handle应有效"), H.IsValid());
    return true;
}
#endif
