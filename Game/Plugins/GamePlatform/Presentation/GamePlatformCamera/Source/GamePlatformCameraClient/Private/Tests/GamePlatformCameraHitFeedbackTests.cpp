#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"

/**
 * 平台镜头舒适度测试：玩家可将命中CameraShake完全关闭；
 * 非有限输入不破坏上一次设置。此纯参数测试不代表视觉震动曲线已验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformHitCameraScaleTest,
    "GamePlatform.Camera.HitFeedback.UserScale",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformHitCameraScaleTest::RunTest(const FString&)
{
    UGamePlatformCameraHitFeedbackSubsystem* Subsystem =
        NewObject<UGamePlatformCameraHitFeedbackSubsystem>(GetTransientPackage());
    TestNotNull(TEXT("可以创建不关联真实Player的测试对象"), Subsystem);
    if (!Subsystem)
    {
        return false;
    }

    Subsystem->SetUserCameraShakeScale(0.0f);
    TestEqual(TEXT("可完整关闭命中震动"), Subsystem->GetUserCameraShakeScale(), 0.0f);
    Subsystem->SetUserCameraShakeScale(5.0f);
    TestEqual(TEXT("强度超限裁剪为1"), Subsystem->GetUserCameraShakeScale(), 1.0f);
    Subsystem->SetUserCameraShakeScale(-2.0f);
    TestEqual(TEXT("负倍率裁剪为0"), Subsystem->GetUserCameraShakeScale(), 0.0f);
    return true;
}
#endif
