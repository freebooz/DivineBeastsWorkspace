#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformSlotWidget.h"
#include "ViewModels/GamePlatformUISnapshotScope.h"

/** 通用槽位基类视觉值比较，无网络、内容资产及新UI组件。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUISlotCommonPresentationTest,
    "GamePlatform.UI.Components.SlotCommonPresentation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUISlotCommonPresentationTest::RunTest(const FString&)
{
    TestTrue(TEXT("通用槽位应继承既有平台组件基类"),
        UGamePlatformSlotWidget::StaticClass()->IsChildOf(
            UGamePlatformComponentWidget::StaticClass()));
    FGamePlatformUISlotState A;
    FGamePlatformUISlotState B;
    TestTrue(TEXT("默认槽位表现相同"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B));
    A.SlotId = TEXT("Test.Slot");
    A.ContentId = TEXT("Test.Content");
    A.Count = 3;
    A.OverlayProgress = 0.4f;
    B = A;
    TestTrue(TEXT("相同状态不重复通知"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B));
    B.Count = 4;
    TestFalse(TEXT("通用数量变化刷新槽位"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B));
    TestTrue(TEXT("不展示数量的技能栏仍不重复广播"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B, false));
    B = A;
    B.bPending = true;
    TestFalse(TEXT("待确认状态变化应通知"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B, false));
    B = A;
    B.OverlayProgress = 0.40005f;
    TestTrue(TEXT("小于旧阈值的遮罩波动不通知"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B));
    B.OverlayProgress = 0.7f;
    TestFalse(TEXT("真实冷却进度变化通知"),
        UGamePlatformSlotWidget::AreSlotStatesEquivalent(A, B));
    FGamePlatformUISlotCellPresentation Cell;
    TestFalse(TEXT("公共槽位默认不可操作"), Cell.bEnabled);
    TestFalse(TEXT("不误覆盖项目已有提示"), Cell.bHasToolTip);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUISnapshotScopeTest,
    "GamePlatform.UI.ViewModel.ScopedSnapshot",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUISnapshotScopeTest::RunTest(const FString&)
{
    const FGuid A = FGuid::NewGuid();
    const FGuid B = FGuid::NewGuid();
    TestFalse(TEXT("无有效绑定来源不可接受状态"),
        FGamePlatformUISnapshotScope::CanAccept(FGuid(), -1, A, 0));
    TestFalse(TEXT("不能接受无效来源"),
        FGamePlatformUISnapshotScope::CanAccept(A, -1, FGuid(), 0));
    TestFalse(TEXT("跨账号或世界的旧通知拒绝"),
        FGamePlatformUISnapshotScope::CanAccept(A, 3, B, 4));
    TestFalse(TEXT("非法修订号拒绝"),
        FGamePlatformUISnapshotScope::CanAccept(A, -1, A, -1));
    TestTrue(TEXT("第一个0号修订可接受"),
        FGamePlatformUISnapshotScope::CanAccept(A, -1, A, 0));
    TestFalse(TEXT("重复修订拒绝"),
        FGamePlatformUISnapshotScope::CanAccept(A, 3, A, 3));
    TestFalse(TEXT("倒退修订拒绝"),
        FGamePlatformUISnapshotScope::CanAccept(A, 3, A, 2));
    TestTrue(TEXT("当前来源新版有效"),
        FGamePlatformUISnapshotScope::CanAccept(A, 3, A, 4));
    return true;
}

#endif
