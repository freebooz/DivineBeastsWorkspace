#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXAssetValidationSmokeTest, "GamePlatform.VFX.Editor.Validation.Smoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXAssetValidationSmokeTest::RunTest(const FString&)
{
    AddInfo(TEXT("DataValidation C++ Validators 会由编辑器自动发现；CI 建议执行 -run=DataValidation。"));
    return true;
}
#endif
