#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Buffer/GamePlatformActionInputBuffer.h"
#include "Services/GamePlatformInputServices.h"

/**
 * 输入缓冲回归：验证事件先后顺序、时效、重复、错误绑定代次、失焦原因过滤。
 * 静态/自动化可证明队列行为；不能替代真实GAS取消窗口与多人同步操作测试。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformActionInputBufferTest,
    "GamePlatform.Input.ActionBuffer.Hitstop",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformActionInputBufferTest::RunTest(const FString&)
{
    FGamePlatformActionInputBuffer Buffer(2, 0.25);
    FGamePlatformInputEvent Press;
    Press.SemanticId.Tag = GamePlatformInputServices::GetBuiltInSemanticTag(
        EGamePlatformBuiltInInputSemantic::Confirm);
    Press.Phase = ETriggerEvent::Started;
    Press.BindingGeneration = 7;
    Press.Sequence = 10;

    TestTrue(TEXT("输入语义有效"), Press.SemanticId.IsValid());
    TestTrue(TEXT("按下事件进入缓冲"), Buffer.Enqueue(Press, 100.0));
    TestFalse(TEXT("相同序号不重复入队"), Buffer.Enqueue(Press, 100.01));
    FGamePlatformInputEvent Release = Press;
    Release.Phase = ETriggerEvent::Completed;
    Release.EndReason = EGamePlatformInputEndReason::NativeCompleted;
    Release.Sequence = 11;
    TestTrue(TEXT("释放事件保留顺序"), Buffer.Enqueue(Release, 100.02));

    TArray<FGamePlatformInputEvent> Out;
    TestEqual(TEXT("错误Binding代次不得执行"), Buffer.ConsumePending(100.08, 6, Out), 0);
    TestEqual(TEXT("错误代次清空队列"), Buffer.Num(), 0);
    TestTrue(TEXT("再次入队"), Buffer.Enqueue(Press, 101.0));
    TestTrue(TEXT("再次保留释放事件"), Buffer.Enqueue(Release, 101.01));
    TestEqual(TEXT("有效代次按顺序恢复按下与释放"), Buffer.ConsumePending(101.12, 7, Out), 2);
    TestEqual(TEXT("第一事件必须是Started"), Out[0].Phase, ETriggerEvent::Started);
    TestEqual(TEXT("第二事件必须是Completed"), Out[1].Phase, ETriggerEvent::Completed);

    TestTrue(TEXT("过期前可以入队"), Buffer.Enqueue(Press, 102.0));
    TArray<FGamePlatformInputEvent> Expired;
    TestEqual(TEXT("过期事件不得回放"), Buffer.ConsumePending(102.5, 7, Expired), 0);
    FGamePlatformInputEvent FocusLost = Release;
    FocusLost.Sequence = 12;
    FocusLost.EndReason = EGamePlatformInputEndReason::FocusLost;
    TestFalse(TEXT("失焦撤销事件不允许延迟回放"), Buffer.Enqueue(FocusLost, 103.0));
    return true;
}
#endif
