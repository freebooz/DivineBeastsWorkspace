#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformVFXCommonParameters.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCommonParametersTest,
    "GamePlatform.VFX.Parameters.CommonNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCommonParametersTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("主色参数名稳定"), GamePlatformVFXCommonParameters::PrimaryColor(), FName(TEXT("User.PrimaryColor")));
    TestEqual(TEXT("强度参数名稳定"), GamePlatformVFXCommonParameters::Intensity(), FName(TEXT("User.Intensity")));
    TestEqual(TEXT("持续时间参数名稳定"), GamePlatformVFXCommonParameters::Duration(), FName(TEXT("User.Duration")));
    TestEqual(TEXT("半径参数名稳定"), GamePlatformVFXCommonParameters::Radius(), FName(TEXT("User.Radius")));
    TestEqual(TEXT("速度参数名稳定"), GamePlatformVFXCommonParameters::Speed(), FName(TEXT("User.Speed")));
    TestEqual(TEXT("源位置参数名稳定"), GamePlatformVFXCommonParameters::SourcePosition(), FName(TEXT("User.SourcePosition")));
    TestEqual(TEXT("目标位置参数名稳定"), GamePlatformVFXCommonParameters::TargetPosition(), FName(TEXT("User.TargetPosition")));
    return true;
}

#endif
