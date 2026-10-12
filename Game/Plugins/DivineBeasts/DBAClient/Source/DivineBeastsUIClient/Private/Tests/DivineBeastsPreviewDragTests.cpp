// 项目客户端本地预览手势回归：横向拖动方向、90度限幅、有限值及捕获身份使用真实共享状态验证。
// 只在UE自动化测试中操作栈上状态；取消、跨指针输入不得影响当前预览，不模拟后端或世界进入。
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
    TestEqual(TEXT("向右拖动100逻辑像素产生反向35度旋转"), State.YawForMove(1,2,100), -35.0f);
    TestEqual(TEXT("其他用户不得控制"), State.YawForMove(0,2,100), 0.0f);
    TestEqual(TEXT("其他指针不得控制"), State.YawForMove(1,0,100), 0.0f);
    TestEqual(TEXT("异常输入不传播"), State.YawForMove(1,2,std::numeric_limits<float>::quiet_NaN()), 0.0f);
    TestEqual(TEXT("向右坐标突变限制为反向90度"), State.YawForMove(1,2,10000), -90.0f);
    State.Cancel();
    TestEqual(TEXT("失活/丢失捕获后旧事件无效"), State.YawForMove(1,2,100), 0.0f);
    return true;
}

// 双向拖动必须反转Yaw而保持每逻辑像素0.35度；零位移不产生旋转，正负输入对称。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPreviewDragDirectionTest,
    "DivineBeasts.UI.Characters.PreviewDragDirection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPreviewDragDirectionTest::RunTest(const FString&)
{
    FDivineBeastsPreviewDragState State;
    State.Begin(1,2);
    TestEqual(TEXT("向右拖动产生负Yaw"), State.YawForMove(1,2,40.0f), -14.0f);
    TestEqual(TEXT("向左拖动产生正Yaw"), State.YawForMove(1,2,-40.0f), 14.0f);
    TestEqual(TEXT("零位移不产生旋转"), State.YawForMove(1,2,0.0f), 0.0f);
    State.Cancel();
    State.Begin(3,4);
    TestFalse(TEXT("重新捕获后旧用户和指针已失去所有权"), State.Owns(1,2));
    TestEqual(TEXT("重新捕获后拒绝旧指针位移"), State.YawForMove(1,2,40.0f), 0.0f);
    TestEqual(TEXT("新捕获保持反向拖动"), State.YawForMove(3,4,40.0f), -14.0f);
    return true;
}

// 反向后仍拒绝NaN/正负无穷；极大但有限输入按两侧90度限幅，不传播异常浮点。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPreviewDragFiniteClampTest,
    "DivineBeasts.UI.Characters.PreviewDragFiniteClamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPreviewDragFiniteClampTest::RunTest(const FString&)
{
    FDivineBeastsPreviewDragState State;
    State.Begin(1,2);
    TestEqual(TEXT("NaN输入不产生旋转"), State.YawForMove(1,2,std::numeric_limits<float>::quiet_NaN()), 0.0f);
    TestEqual(TEXT("正无穷输入不产生旋转"), State.YawForMove(1,2,std::numeric_limits<float>::infinity()), 0.0f);
    TestEqual(TEXT("负无穷输入不产生旋转"), State.YawForMove(1,2,-std::numeric_limits<float>::infinity()), 0.0f);
    TestEqual(TEXT("向右极大有限输入限制为负90度"), State.YawForMove(1,2,std::numeric_limits<float>::max()), -90.0f);
    TestEqual(TEXT("向左极大有限输入限制为正90度"), State.YawForMove(1,2,-std::numeric_limits<float>::max()), 90.0f);
    TestEqual(TEXT("限幅不得给其他用户授权"), State.YawForMove(3,2,-10000.0f), 0.0f);
    TestEqual(TEXT("限幅不得给其他指针授权"), State.YawForMove(1,4,-10000.0f), 0.0f);
    State.Cancel();
    TestEqual(TEXT("取消后大位移也不得产生旋转"), State.YawForMove(1,2,-10000.0f), 0.0f);
    return true;
}
#endif
