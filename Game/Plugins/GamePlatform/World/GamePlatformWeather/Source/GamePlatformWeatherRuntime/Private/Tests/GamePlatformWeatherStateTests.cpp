// 自动化测试只证明纯数值、输入边界与量化插值；不冒充联机或真实粒子验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformWeatherTypes.h"
#include <limits> // 标准C++产生NaN测试输入；UE的TNumericLimits未定义QuietNaN。

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherValuesTest,
    "GamePlatform.Weather.Runtime.QuantizationAndTransition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherValuesTest::RunTest(const FString&)
{
    FGamePlatformWeatherState Rain;
    Rain.Type = EGamePlatformWeatherType::HeavyRain;
    Rain.RainIntensity = 0.72f;
    Rain.Wetness = 0.83f;
    Rain.PuddleAmount = 0.24f;
    Rain.TemperatureCelsius = 12.3f;
    TestTrue(TEXT("合法天气"), Rain.IsValid());
    const FGamePlatformWeatherState Restored = FGamePlatformWeatherQuantizedState::Encode(Rain).Decode();
    TestEqual(TEXT("天气类型"), Restored.Type, EGamePlatformWeatherType::HeavyRain);
    TestTrue(TEXT("强度量化误差在1/255以内"), FMath::Abs(Restored.RainIntensity - Rain.RainIntensity) <= 1.f / 255.f);
    TestTrue(TEXT("温度精度0.1摄氏度"), FMath::IsNearlyEqual(Restored.TemperatureCelsius, 12.3f, .01f));

    FGamePlatformWeatherSnapshot Snapshot;
    Snapshot.From = FGamePlatformWeatherQuantizedState::Encode(FGamePlatformWeatherState());
    Snapshot.To = FGamePlatformWeatherQuantizedState::Encode(Rain);
    Snapshot.Revision = 2;
    Snapshot.StartedAtServerSeconds = 100.f;
    Snapshot.TransitionSeconds = 10.f;
    TestTrue(TEXT("半途雨量正确"), FMath::IsNearlyEqual(Snapshot.Sample(105.f).RainIntensity, Restored.RainIntensity * .5f, .01f));
    TestTrue(TEXT("过渡完成雨量保持目标"), FMath::IsNearlyEqual(Snapshot.Sample(111.f).RainIntensity, Restored.RainIntensity, .001f));
    FGamePlatformWeatherState Invalid = Rain;
    Invalid.SnowAmount = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("非有限值拒绝"), Invalid.IsValid());
    Invalid = Rain;
    Invalid.RainIntensity = 1.01f;
    TestFalse(TEXT("超范围值拒绝"), Invalid.IsValid());
    FGamePlatformWeatherScheduleEntry Entry;
    Entry.MinHoldSeconds = 120.f;
    Entry.MaxHoldSeconds = 60.f;
    TestFalse(TEXT("倒置时段拒绝"), Entry.IsValid());
    return true;
}
#endif
