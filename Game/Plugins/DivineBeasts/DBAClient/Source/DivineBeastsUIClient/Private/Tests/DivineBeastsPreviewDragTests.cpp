// 本地预览手势回归：取消、跨指针输入及异常坐标不得影响当前预览角色；不模拟后端或世界进入。
#include "Screens/Characters/DivineBeastsPreviewDragState.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPreviewDragTest,
    "DivineBeasts.UI.Characters.PreviewDragLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPreviewDragTest::RunTest(const FString&)
{
    FDivineBeastsPreviewDragState State;
    TestEqual(TEXT("未按下鼠标不旋转"), State.YawForMove(0,0,100), 0.0f);
    State.Begin(1,2);
    TestEqual(TEXT("按逻辑像素旋转"), State.YawForMove(1,2,100), 35.0f);
    TestEqual(TEXT("其他用户不得控制"), State.YawForMove(0,2,100), 0.0f);
    TestEqual(TEXT("其他指针不得控制"), State.YawForMove(1,0,100), 0.0f);
    TestEqual(TEXT("异常输入不传播"), State.YawForMove(1,2,std::numeric_limits<float>::quiet_NaN()), 0.0f);
    TestEqual(TEXT("重新捕获坐标突变有界"), State.YawForMove(1,2,10000), 90.0f);
    State.Cancel();
    TestEqual(TEXT("失活/丢失捕获后旧事件无效"), State.YawForMove(1,2,100), 0.0f);
    return true;
}
#endif
