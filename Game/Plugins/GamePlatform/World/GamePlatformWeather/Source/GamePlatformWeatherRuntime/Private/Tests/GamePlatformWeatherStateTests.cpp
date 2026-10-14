// 自动化测试只证明纯数值、输入边界与量化插值；不冒充联机或真实粒子验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
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

/**
 * 天气广播允许同步重配并立即启动下一代计划。
 * 原实现持有被Schedule赋值失效的元素指针，且会以旧15秒Timer覆盖新1秒Timer。
 * 本测试在Game世界调用真实Replicator和TimerManager，不把状态机静态文本当成运行成功。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherScheduleReentryTest,
    "GamePlatform.Weather.Runtime.ReentrantScheduleReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherScheduleReentryTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("真实测试世界创建"), World)) return false;
    UGamePlatformWeatherWorldSubsystem* Service = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    if (!TestNotNull(TEXT("世界天气服务存在"), Service))
    {
        World->DestroyWorld(false);
        return false;
    }

    FGamePlatformWeatherScheduleEntry OldEntry;
    OldEntry.State.Type = EGamePlatformWeatherType::HeavyRain;
    OldEntry.State.RainIntensity = 0.8f;
    OldEntry.MinHoldSeconds = 15.f;
    OldEntry.MaxHoldSeconds = 15.f;
    OldEntry.TransitionSeconds = 0.f;

    FGamePlatformWeatherScheduleEntry NewEntry;
    NewEntry.State.Type = EGamePlatformWeatherType::HeavySnow;
    NewEntry.State.SnowIntensity = 0.7f;
    NewEntry.MinHoldSeconds = 1.f;
    NewEntry.MaxHoldSeconds = 1.f;
    NewEntry.TransitionSeconds = 0.f;

    TArray<FGamePlatformWeatherScheduleEntry> OldSchedule;
    OldSchedule.Add(OldEntry);
    TArray<FGamePlatformWeatherScheduleEntry> NewSchedule;
    NewSchedule.Add(NewEntry);
    if (!TestTrue(TEXT("天气权威Actor成功激活"),
                  Service->ActivateWeather(FGamePlatformWeatherState())))
    {
        World->DestroyWorld(false);
        return false;
    }

    bool bReconfigured = false;
    bool bRestarted = false;
    const FDelegateHandle Handler = Service->AddSnapshotHandler(
        FGamePlatformWeatherSnapshotChanged::FDelegate::CreateLambda(
            [Service, &NewSchedule, &bReconfigured, &bRestarted](const FGamePlatformWeatherSnapshot& Snapshot)
            {
                if (Snapshot.Revision != 2) return;
                bReconfigured = Service->ConfigureSchedule(NewSchedule, 17);
                if (bReconfigured) bRestarted = Service->StartSchedule();
            }));

    TestTrue(TEXT("注册非网络天气快照回调"), Handler.IsValid());
    TestTrue(TEXT("安装旧15秒天气计划"), Service->ConfigureSchedule(OldSchedule, 11));
    TestTrue(TEXT("启动自动天气（含回调内新计划重启）"), Service->StartSchedule());
    TestTrue(TEXT("同步回调成功重配"), bReconfigured);
    TestTrue(TEXT("新一代计划已启动"), bRestarted);
    TestEqual(TEXT("立即发布旧计划与新计划两次快照"), Service->GetCurrentSnapshot().Revision, 3);
    TestEqual(TEXT("新天气类型已生效"),
              Service->GetCurrentSnapshot().To.Type, EGamePlatformWeatherType::HeavySnow);

    // 新计划只等待1秒；若旧发布栈覆盖同一TimerHandle为15秒，此处不会生成Revision=4。
    World->GetTimerManager().Tick(1.2f);
    TestTrue(TEXT("旧调度不得覆盖新一代1秒Timer"), Service->GetCurrentSnapshot().Revision >= 4);

    Service->RemoveSnapshotHandler(Handler);
    Service->StopSchedule();
    World->DestroyWorld(false);
    return true;
}
#endif
