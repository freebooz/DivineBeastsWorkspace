#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Time/GamePlatformLiveOpsServerTimeEstimator.h"
#include "Types/GamePlatformLiveOpsTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformLiveOpsServerTimeTest,
    "GamePlatform.LiveOps.Client.ServerTime",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformLiveOpsServerTimeTest::RunTest(const FString&)
{
    FGamePlatformLiveOpsServerTimeEstimator Estimator;
    const FDateTime ServerTime(
        2000, 1, 1, 12, 0, 0);

    Estimator.Update(ServerTime);

    TestTrue(TEXT("ServerTime estimator有效"), Estimator.IsValid());

    const FDateTime Estimated =
        Estimator.EstimatedServerNowUtc();

    TestTrue(
        TEXT("估算时间来自Server snapshot而非本机Wall Clock"),
        Estimated.GetYear() == 2000);

    FGamePlatformLiveOpsTimeWindow Window;
    Window.StartsAtUtc = ServerTime;
    Window.bHasEnd = true;
    Window.EndsAtUtc = ServerTime + FTimespan::FromHours(1);

    TestTrue(
        TEXT("窗口开始边界有效"),
        Window.IsActive(ServerTime));

    TestFalse(
        TEXT("窗口结束边界为exclusive"),
        Window.IsActive(Window.EndsAtUtc));

    return true;
}

#endif
