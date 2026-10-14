// GamePlatformWater运行层纯数学测试：在Editor自动化验证旋转体积、水深边界、流速和非法配置。
// 不调用GPU/Niagara/FluidFlux，不依赖正式Village地图加载成功。
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformWaterTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWaterSamplingTests,
    "GamePlatform.Water.Runtime.RegionSampling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformWaterSamplingTests::RunTest(const FString& Parameters)
{
    FGamePlatformWaterRegionConfig Config;
    Config.RegionId = TEXT("Review.TestLake");
    Config.ShallowDepthCm = 90.f;
    Config.FlowDirection = FVector2D(3.f, 4.f);
    Config.FlowSpeedCmPerSecond = 100.f;
    const FTransform Box(FRotator(0.f, 45.f, 0.f), FVector(0.f, 0.f, -200.f));
    const FVector Bounds(1000.f, 1000.f, 200.f);
    FGamePlatformWaterSample Result;
    TestTrue(TEXT("中心近岸水下40cm进入水域"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(0.f, 0.f, -40.f), Config, Result));
    TestTrue(TEXT("采样结果应处于水体"), Result.bInWater);
    TestTrue(TEXT("40cm为浅水"), Result.bShallow);
    TestTrue(TEXT("读取有主键的区域"), Result.RegionId == Config.RegionId);
    TestTrue(TEXT("流速X归一化乘100"), FMath::IsNearlyEqual(Result.FlowVelocityCmPerSecond.X, 60.f));
    TestTrue(TEXT("流速Y归一化乘100"), FMath::IsNearlyEqual(Result.FlowVelocityCmPerSecond.Y, 80.f));

    TestFalse(TEXT("水面以上不应判为浸水"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(0.f, 0.f, 10.f), Config, Result));
    TestFalse(TEXT("失败后不得留下旧结果"), Result.bInWater);

    TestFalse(TEXT("远离XY边界不在区域内"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(5000.f, 0.f, -40.f), Config, Result));
    TestFalse(TEXT("穿过水底边界后不属于该静态区域"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(0.f, 0.f, -2000.f), Config, Result));
    Config.RegionId = NAME_None;
    TestFalse(TEXT("缺稳定区域ID绝不可参与权威玩法"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(0.f, 0.f, -40.f), Config, Result));
    Config.RegionId = TEXT("Review.TestLake");
    Config.FlowSpeedCmPerSecond = 2000.f;
    TestFalse(TEXT("超出流速上限不得自动截断权威数据"),
        FGamePlatformWaterMath::SampleWaterVolume(Box, Bounds, 0.f,
            FVector(0.f, 0.f, -40.f), Config, Result));
    return true;
}
#endif
