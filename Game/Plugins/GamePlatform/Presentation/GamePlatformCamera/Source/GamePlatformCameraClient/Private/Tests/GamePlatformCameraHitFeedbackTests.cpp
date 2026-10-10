#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"

namespace
{
    /**
     * 仅测试使用的本地玩家作用域，验证镜头舒适度倍率参数；不创建实际玩家控制器、
     * 世界、视口或镜头播放，不触发 PlayerAdded/自动依赖初始化。即使只测纯参数/缓存，
     * 引擎 ClassWithin 仍要求 Engine → LocalPlayer → Subsystem 的合法 Outer。
     * 强持有宿主和子系统，所有退出先 Deinitialize，再释放子系统和玩家。
     */
    struct FCameraLocalPlayerFixture
    {
        TStrongObjectPtr<ULocalPlayer> Player;
        TStrongObjectPtr<UGamePlatformCameraHitFeedbackSubsystem> Subsystem;

        bool Initialize(FAutomationTestBase& Test)
        {
            if (!Test.TestNotNull(TEXT("本地玩家夹具需要真实Engine宿主"), GEngine))
            {
                return false;
            }
            Player.Reset(NewObject<ULocalPlayer>(GEngine));
            if (!Test.TestNotNull(TEXT("本地玩家具有合法Engine Outer"), Player.Get()))
            {
                return false;
            }
            Subsystem.Reset(NewObject<UGamePlatformCameraHitFeedbackSubsystem>(Player.Get()));
            return Test.TestNotNull(TEXT("子系统具有合法LocalPlayer Outer"), Subsystem.Get());
        }

        ~FCameraLocalPlayerFixture()
        {
            if (Subsystem.IsValid())
            {
                Subsystem->Deinitialize();
            }
            Subsystem.Reset();
            Player.Reset();
        }
    };
}

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
    FCameraLocalPlayerFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformCameraHitFeedbackSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
    TestNotNull(TEXT("合法LocalPlayer作用域可以创建镜头参数测试对象"), Subsystem);
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
