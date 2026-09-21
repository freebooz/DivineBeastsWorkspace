#include "Loading/DataNextTick.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
// 使用真实FTSTicker而非调度替身；恢复零延迟一次性执行会使第二轮计数直接跳到32。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDataNextTickTest, "GamePlatform.Data.Runtime.DeferredRequeueZeroDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDataNextTickTest::RunTest(const FString& Parameters)
{
    FTSTicker Ticker;
    int32 Attempts = 0;
    bool bIsDiscovered = false;
    TFunction<void()> Retry;
    Retry = [&]()
    {
        ++Attempts;
        // 有限次数防止回归版本永久挂死测试；32只是测试保险，不属于生产发现预算。
        if (!bIsDiscovered && Attempts < 32) GamePlatform::Data::NextTick(Retry, Ticker);
    };
    GamePlatform::Data::NextTick(Retry, Ticker);
    Ticker.Tick(0.0f);
    TestEqual(TEXT("首轮只登记，不执行Work"), Attempts, 0);
    Ticker.Tick(0.0f);
    TestEqual(TEXT("持续未发现仅执行一次并延后下一次重排"), Attempts, 1);
    Ticker.Tick(0.0f);
    TestEqual(TEXT("零Delta也可推进独立调度轮"), Attempts, 2);
    bIsDiscovered = true;
    Ticker.Tick(0.0f);
    TestEqual(TEXT("条件就绪后最后一次工作结束"), Attempts, 3);
    Ticker.Tick(0.0f);
    TestEqual(TEXT("没有遗留重排"), Attempts, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDataNestedTickerTest, "GamePlatform.Data.Runtime.NestedTickerNotification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDataNestedTickerTest::RunTest(const FString& Parameters)
{
    FTSTicker Ticker;
    bool bHasNotified = false;
    Ticker.AddTicker(FTickerDelegate::CreateLambda([&](float)
    {
        GamePlatform::Data::NextTick([&]() { bHasNotified = true; }, Ticker);
        return false;
    }));
    Ticker.Tick(0.0f);
    TestFalse(TEXT("Ticker回调内提交不能同轮通知"), bHasNotified);
    Ticker.Tick(0.0f);
    TestTrue(TEXT("下一外层Tick才能通知"), bHasNotified);
    return true;
}
#endif
