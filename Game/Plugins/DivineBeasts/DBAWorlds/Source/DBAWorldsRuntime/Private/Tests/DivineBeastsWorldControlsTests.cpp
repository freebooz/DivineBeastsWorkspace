// 第三层世界操控纯策略回归：验证左右键、A/D修饰及暂停清理；不证明视口、网络或真实资源运行。
// 独立原生编译显式定义此宏；UE目标没有该宏时按0处理，不降低C4668未定义宏门禁。
#ifndef DBA_WORLD_CONTROLS_STANDALONE
#define DBA_WORLD_CONTROLS_STANDALONE 0
#endif
#if DBA_WORLD_CONTROLS_STANDALONE || WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/DivineBeastsWorldControlState.h"

#if DBA_WORLD_CONTROLS_STANDALONE
#include <cassert>
#include <iostream>
#define DBA_CHECK(Expression) assert(Expression)
#else
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldControlsTest, "DivineBeasts.World.Controls.MouseAndMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
#define DBA_CHECK(Expression) TestTrue(TEXT(#Expression), Expression)
bool FDivineBeastsWorldControlsTest::RunTest(const FString&)
#endif
#if DBA_WORLD_CONTROLS_STANDALONE
int main()
#endif
{
    // 默认鼠标可用；仅左键观察，右键控制朝向，双键前进只在世界Active时成立。
    FDivineBeastsWorldControlState State;
    // GameAndUI未处理不等于世界空白：禁用按钮、RMB未处理和其它视口都不允许捕获。
    DBA_CHECK(FDivineBeastsWorldControlState::CanBeginViewportDrag(true, true));
    DBA_CHECK(!FDivineBeastsWorldControlState::CanBeginViewportDrag(true, false));
    DBA_CHECK(!FDivineBeastsWorldControlState::CanBeginViewportDrag(false, true));
    DBA_CHECK(!FDivineBeastsWorldControlState::CanBeginViewportDrag(false, false));
    // 本控制器持有的输入锁只在需要变化时增减；其它调用方仍持有一个锁时不得清零。
    bool bOwnsLock = false;
    int ExternalAndWorldLockCount = 1;
    ExternalAndWorldLockCount += FDivineBeastsWorldControlState::UpdateOwnedInputLock(true, bOwnsLock);
    DBA_CHECK(ExternalAndWorldLockCount == 2 && bOwnsLock);
    ExternalAndWorldLockCount += FDivineBeastsWorldControlState::UpdateOwnedInputLock(true, bOwnsLock);
    DBA_CHECK(ExternalAndWorldLockCount == 2);
    ExternalAndWorldLockCount += FDivineBeastsWorldControlState::UpdateOwnedInputLock(false, bOwnsLock);
    DBA_CHECK(ExternalAndWorldLockCount == 1 && !bOwnsLock);
    ExternalAndWorldLockCount += FDivineBeastsWorldControlState::UpdateOwnedInputLock(false, bOwnsLock);
    DBA_CHECK(ExternalAndWorldLockCount == 1);
    DBA_CHECK(!State.IsDragging());
    State.SetMouseButton(false, true);
    DBA_CHECK(State.IsDragging() && !State.IsFacingCamera() && State.ResolveForward(0.f) == 0.f);
    // 左键拖动只改变观察镜头的水平/俯仰；不能把轴输入写入角色朝向。
    const FDivineBeastsWorldMouseLookDelta LeftMouseLook = State.ResolveMouseLook(6.f, -4.f);
    DBA_CHECK(LeftMouseLook.CameraYawInput == 6.f);
    DBA_CHECK(LeftMouseLook.CameraPitchInput == -4.f);
    DBA_CHECK(LeftMouseLook.CharacterYawInput == 0.f);
    State.SetMouseButton(true, true);
    DBA_CHECK(State.IsFacingCamera() && State.ResolveForward(0.f) == 1.f);
    // 右键沿用相同镜头轴输入，并只把水平转角同步到角色，不把俯仰传给角色。
    const FDivineBeastsWorldMouseLookDelta RightMouseLook = State.ResolveMouseLook(6.f, -4.f);
    DBA_CHECK(RightMouseLook.CameraYawInput == 6.f);
    DBA_CHECK(RightMouseLook.CameraPitchInput == -4.f);
    DBA_CHECK(RightMouseLook.CharacterYawInput == 6.f);
    // 后退和双键同时输入采用显式前进，避免相加后抵消或超过归一化输入范围。
    DBA_CHECK(State.ResolveForward(-1.f) == 1.f);
    DBA_CHECK(State.ResolveTurn(1.f) == 0.f && State.ResolveStrafe(0.f, 1.f) == 1.f);
    State.SetMouseButton(true, false);
    DBA_CHECK(State.IsDragging() && !State.IsFacingCamera());
    DBA_CHECK(State.ResolveTurn(-1.f) == -1.f && State.ResolveStrafe(1.f, -1.f) == 1.f);
    State.Reset();
    DBA_CHECK(!State.IsDragging() && State.ResolveForward(0.f) == 0.f);
    State.SetMouseButton(true, true);
    DBA_CHECK(State.ResolveStrafe(1.f, 1.f) == 1.f);
    DBA_CHECK(State.ResolveStrafe(-1.f, -1.f) == -1.f);
    State.Reset();
    DBA_CHECK(!State.IsFacingCamera() && State.ResolveTurn(1.f) == 1.f);
#if DBA_WORLD_CONTROLS_STANDALONE
    std::cout << "WORLD_CONTROLS_POLICY_PASS\n";
    return 0;
#else
    return true;
#endif
}
#undef DBA_CHECK
#endif
