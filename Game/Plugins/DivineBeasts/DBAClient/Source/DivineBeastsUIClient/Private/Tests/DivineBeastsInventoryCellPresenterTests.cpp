#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Screens/Inventory/DivineBeastsInventoryCellPresenter.h"

/** 纯显示转换回归：不创建世界或服务器、不编辑蓝图，也不触发背包写入。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsInventoryCellPresenterTest,
    "DivineBeasts.UI.Inventory.CellPresenter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsInventoryCellPresenterTest::RunTest(const FString&)
{
    using FPresenter = FDivineBeastsInventoryCellPresenter;
    const FString SelectedId(TEXT("item-a"));

    // 当前容器页面尚未提供对应槽位时不得伪造物品或允许点击。
    const auto Missing = FPresenter::MakeInventoryCell(nullptr, false, true, SelectedId);
    TestTrue(TEXT("缺失格为空文案"), Missing.LabelText.IsEmpty());
    TestFalse(TEXT("缺失格禁止操作"), Missing.bEnabled);
    TestEqual(TEXT("缺失格提供空格提示"), Missing.ToolTip.ToString(), FString(TEXT("空背包格")));

    FGamePlatformInventorySlotViewModel Slot;
    Slot.ContainerId = TEXT("main");
    Slot.SlotIndex = 4;
    Slot.bOccupied = false;
    auto Empty = FPresenter::MakeInventoryCell(&Slot, false, true, SelectedId);
    TestEqual(TEXT("空格索引保留两位编号"), Empty.LabelText, FString(TEXT("05")));
    TestTrue(TEXT("服务就绪时真实空格可操作"), Empty.bEnabled);
    Empty = FPresenter::MakeInventoryCell(&Slot, false, false, SelectedId);
    TestFalse(TEXT("服务未就绪时空格禁用"), Empty.bEnabled);

    Slot.bOccupied = true;
    Slot.Item.ItemInstanceId = SelectedId;
    Slot.Item.ItemDefinitionId = TEXT("DBA_Inventory_AuditItem");
    Slot.Item.Quantity = 23;
    Slot.Item.bPending = true;
    const auto WithIcon = FPresenter::MakeInventoryCell(&Slot, true, true, SelectedId);
    TestEqual(TEXT("已加载图标只显示数量和未决标记"), WithIcon.LabelText,
        FString(TEXT("×23…")));
    TestTrue(TEXT("已选中项使用唯一选中背景"), WithIcon.BackgroundColor.Equals(
        FLinearColor(0.34f, 0.55f, 0.40f, 1.0f)));
    TestTrue(TEXT("占用格仍有可读工具提示"), !WithIcon.ToolTip.IsEmpty());

    const auto NoIcon = FPresenter::MakeInventoryCell(&Slot, false, true, FString());
    TestTrue(TEXT("无图标时保留数量后备"), NoIcon.LabelText.Contains(TEXT("\n×23…")));
    TestFalse(TEXT("不同物品不误标为选中"), NoIcon.BackgroundColor.Equals(
        WithIcon.BackgroundColor));

    FGamePlatformInventoryQuickbarViewModel Quick;
    Quick.SlotIndex = 2;
    const auto QuickEmpty = FPresenter::MakeQuickbarCell(&Quick, 2, false, true, 2);
    TestEqual(TEXT("未绑定快捷栏保留位序"), QuickEmpty.LabelText, FString(TEXT("3")));
    TestFalse(TEXT("快捷栏不覆盖旧的工具提示规则"), QuickEmpty.bHasToolTip);
    Quick.bAssigned = true;
    Quick.bPending = true;
    Quick.Item.ItemDefinitionId = Slot.Item.ItemDefinitionId;
    Quick.Item.Quantity = 7;
    const auto QuickIcon = FPresenter::MakeQuickbarCell(&Quick, 2, true, true, 2);
    TestEqual(TEXT("快捷栏图标就绪数量和未决标记"), QuickIcon.LabelText,
        FString(TEXT("×7…")));
    TestTrue(TEXT("快捷栏焦点背景不丢失"), QuickIcon.BackgroundColor.Equals(
        FLinearColor(0.38f, 0.48f, 0.28f, 1.0f)));
    const auto QuickFallback = FPresenter::MakeQuickbarCell(&Quick, 2, false, false, 1);
    TestTrue(TEXT("快捷栏无图标中文后备包含数量"), QuickFallback.LabelText.Contains(TEXT("\n×7…")));
    TestFalse(TEXT("快捷栏服务未就绪不可点击"), QuickFallback.bEnabled);
    const auto InvalidQuick = FPresenter::MakeQuickbarCell(&Quick, 12, true, true, 12);
    TestFalse(TEXT("快捷栏越界不可操作"), InvalidQuick.bEnabled);
    return true;
}

#endif
