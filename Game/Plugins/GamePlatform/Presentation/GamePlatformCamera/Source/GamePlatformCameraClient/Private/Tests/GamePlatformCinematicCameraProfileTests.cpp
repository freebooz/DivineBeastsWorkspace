#if WITH_DEV_AUTOMATION_TESTS
// 纯参数回归，不冒充电影摄像机资产或真实视口验证。
#include "Misc/AutomationTest.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCinematicProfileValidationTest,
    "GamePlatform.Camera.Cinematic.Profile.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCinematicProfileValidationTest::RunTest(const FString&)
{
    UGamePlatformCinematicCameraProfile* Profile =
        NewObject<UGamePlatformCinematicCameraProfile>(GetTransientPackage());
    if (!TestNotNull(TEXT("电影镜头配置对象"), Profile))
    {
        return false;
    }
    TestTrue(TEXT("默认参数有效"), Profile->IsValidProfile());
    Profile->FocalLengthMm = 0.0f;
    TestFalse(TEXT("零焦距应拒绝"), Profile->IsValidProfile());
    Profile->FocalLengthMm = 35.0f;
    Profile->BlendTimeSeconds = -1.0f;
    TestFalse(TEXT("负混合时间应拒绝"), Profile->IsValidProfile());
    return true;
}
#endif
