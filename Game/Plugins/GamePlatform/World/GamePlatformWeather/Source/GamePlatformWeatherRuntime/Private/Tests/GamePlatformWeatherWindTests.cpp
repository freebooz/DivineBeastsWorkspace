// 平台天气纯数值回归：统一世界风向、循环量化和阵风连续相位；不证明原生植物渲染。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformWeatherTypes.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherWindTest,
    "GamePlatform.Weather.Runtime.WindDirectionAndPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherWindTest::RunTest(const FString&)
{
    FGamePlatformWeatherState From, To;
    From.WindYawDegrees = 359.f; To.WindYawDegrees = 1.f;
    TestTrue(TEXT("跨零方向走最短弧"), FMath::IsNearlyZero(
        FGamePlatformWeatherState::Interpolate(From, To, .5f).WindYawDegrees, .001f));
    for (const float Yaw : {0.f, 90.f, 180.f, 270.f, 359.999f})
    {
        From.WindYawDegrees = Yaw;
        const auto Restored = FGamePlatformWeatherQuantizedState::Encode(From).Decode();
        TestTrue(TEXT("整圈量化误差"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Restored.WindYawDegrees)) <= .003f);
    }
    From.WindYawDegrees = 360.f;
    TestFalse(TEXT("360度输入拒绝"), From.IsValid());
    From.WindYawDegrees = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("非有限方向拒绝"), From.IsValid());
    From = {}; From.WindGustStrength = 1.01f;
    TestFalse(TEXT("阵风比例越界拒绝"), From.IsValid());
    From = {}; From.WindGustFrequencyHz = 2.01f;
    TestFalse(TEXT("阵风频率越界拒绝"), From.IsValid());
    From = {}; From.WindGustFrequencyHz = .1234f;
    TestTrue(TEXT("频率毫赫兹量化误差"), FMath::Abs(
        FGamePlatformWeatherQuantizedState::Encode(From).Decode().WindGustFrequencyHz - From.WindGustFrequencyHz) <= .0005f);
    From = {}; To = {};
    From.WindGustFrequencyHz = .1f; To.WindGustFrequencyHz = .2f;
    FGamePlatformWeatherSnapshot Snapshot;
    Snapshot.From = FGamePlatformWeatherQuantizedState::Encode(From);
    Snapshot.To = FGamePlatformWeatherQuantizedState::Encode(To);
    Snapshot.Revision = 1; Snapshot.TransitionSeconds = 10.f;
    TestTrue(TEXT("过渡相位积分"), FMath::IsNearlyEqual(Snapshot.SampleWindPhaseCycles(5.f), .625f, .0001f));
    TestTrue(TEXT("迟加入取模相位"), FMath::IsNearlyEqual(Snapshot.SampleWindPhaseCycles(15.f), .5f, .0001f));
    FGamePlatformWeatherSnapshot Interrupted;
    Interrupted.From = FGamePlatformWeatherQuantizedState::Encode(Snapshot.Sample(5.f));
    Interrupted.To = Snapshot.To; Interrupted.Revision = 2;
    Interrupted.StartedAtServerSeconds = 5.f; Interrupted.TransitionSeconds = 3.f;
    Interrupted.WindPhaseAtStartCycles = Snapshot.SampleWindPhaseCycles(5.f);
    TestTrue(TEXT("打断保持相位"), FMath::IsNearlyEqual(Interrupted.SampleWindPhaseCycles(5.f), .625f, .0001f));
    To.WindGustFrequencyHz = 0.f; Snapshot.To = FGamePlatformWeatherQuantizedState::Encode(To);
    Snapshot.TransitionSeconds = 0.f; Snapshot.WindPhaseAtStartCycles = .3f;
    TestTrue(TEXT("零频率冻结"), FMath::IsNearlyEqual(Snapshot.SampleWindPhaseCycles(100.f), .3f));
    return true;
}
#endif
