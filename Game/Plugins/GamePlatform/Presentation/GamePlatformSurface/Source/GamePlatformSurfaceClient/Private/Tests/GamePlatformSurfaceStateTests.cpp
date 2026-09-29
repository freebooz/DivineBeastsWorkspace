// Surface状态纯数据回归：验证裁剪、去重和更新结果语义，不依赖真实渲染资产。
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformSurfaceTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSurfaceStateClampTest,
    "GamePlatform.Surface.State.ClampAndCompare",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSurfaceStateClampTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FGamePlatformSurfaceEnvironmentState Source;
    Source.GlobalWetness = 2.0f;
    Source.GlobalSnowAmount = -1.0f;
    Source.GlobalMossInfluence = 4.0f;
    Source.GlobalPuddleAmount = -3.0f;
    Source.RainIntensity = 1.5f;
    Source.SnowIntensity = -0.5f;
    Source.TemperatureCelsius = 180.0f;

    TestTrue(TEXT("合法有限输入应通过有限值检查"), Source.IsFinite());

    const FGamePlatformSurfaceEnvironmentState Clamped = Source.GetClamped();
    TestEqual(TEXT("湿润裁剪到1"), Clamped.GlobalWetness, 1.0f);
    TestEqual(TEXT("积雪裁剪到0"), Clamped.GlobalSnowAmount, 0.0f);
    TestEqual(TEXT("苔藓影响裁剪到1"), Clamped.GlobalMossInfluence, 1.0f);
    TestEqual(TEXT("积水裁剪到0"), Clamped.GlobalPuddleAmount, 0.0f);
    TestEqual(TEXT("降雨裁剪到1"), Clamped.RainIntensity, 1.0f);
    TestEqual(TEXT("降雪裁剪到0"), Clamped.SnowIntensity, 0.0f);
    TestEqual(TEXT("温度裁剪到100摄氏度"), Clamped.TemperatureCelsius, 100.0f);
    TestTrue(TEXT("相同状态应被识别为无变化"), Clamped.IsNearlyEqual(Clamped));

    FGamePlatformSurfaceUpdateResult Result;
    Result.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
    TestTrue(TEXT("MPC缺失时状态仍可被接受并等待重新绑定"), Result.IsStateAccepted());

    return true;
}

#endif
