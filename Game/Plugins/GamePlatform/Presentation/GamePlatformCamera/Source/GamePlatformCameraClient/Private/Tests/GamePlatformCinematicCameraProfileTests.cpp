#if WITH_DEV_AUTOMATION_TESTS
// 纯参数回归，不冒充电影摄像机资产或真实视口验证。
#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h" // 完整UPackage定义用于测试中明确的瞬态Outer，不依赖其他Unity单元偶然包含。

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCinematicFocalLengthMappingTest,
    "GamePlatform.Camera.Cinematic.FocalLengthMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCinematicFocalLengthMappingTest::RunTest(const FString&)
{
    TestTrue(TEXT("24毫米广角视场角"), FMath::IsNearlyEqual(
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(24.0f, 36.0f),
        73.7398f, 0.01f));
    TestTrue(TEXT("35毫米标准视场角"), FMath::IsNearlyEqual(
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(35.0f, 36.0f),
        54.4322f, 0.01f));
    TestTrue(TEXT("50毫米人像视场角"), FMath::IsNearlyEqual(
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(50.0f, 36.0f),
        39.5978f, 0.01f));
    TestTrue(TEXT("85毫米长焦视场角"), FMath::IsNearlyEqual(
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(85.0f, 36.0f),
        23.9130f, 0.01f));
    TestEqual(TEXT("焦距越长视场角越窄"),
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(85.0f, 36.0f) <
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(24.0f, 36.0f),
        true);
    TestEqual(TEXT("非正焦距安全退化为零"),
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(0.0f, 36.0f),
        0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCinematicGenericCameraOpticsTest,
    "GamePlatform.Camera.Cinematic.GenericCameraOpticsAndArenaIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCinematicGenericCameraOpticsTest::RunTest(const FString&)
{
    UCameraComponent* Camera = NewObject<UCameraComponent>(GetTransientPackage());
    UGamePlatformCinematicCameraProfile* Profile =
        NewObject<UGamePlatformCinematicCameraProfile>(GetTransientPackage());
    if (!TestNotNull(TEXT("普通预览相机组件"), Camera) ||
        !TestNotNull(TEXT("角色展示配置"), Profile))
    {
        return false;
    }

    Profile->Mode = EGamePlatformCameraPresentationMode::CharacterPreview;
    Profile->FocalLengthMm = 35.0f;
    TestTrue(TEXT("普通预览相机可应用电影等效焦段"),
        UGamePlatformCinematicCameraSubsystem::ApplyProfileToCameraComponent(Camera, Profile));
    TestTrue(TEXT("35毫米焦段转换为对应视场角"), FMath::IsNearlyEqual(
        Camera->FieldOfView,
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(35.0f, 36.0f),
        0.01f));

    Camera->SetFieldOfView(88.0f);
    Profile->Mode = EGamePlatformCameraPresentationMode::Arena;
    Profile->FocalLengthMm = 85.0f;
    TestTrue(TEXT("竞技模式配置可安全识别"),
        UGamePlatformCinematicCameraSubsystem::ApplyProfileToCameraComponent(Camera, Profile));
    TestTrue(TEXT("竞技模式不继承角色展示焦段"), FMath::IsNearlyEqual(Camera->FieldOfView, 88.0f));
    return true;
}
#endif
