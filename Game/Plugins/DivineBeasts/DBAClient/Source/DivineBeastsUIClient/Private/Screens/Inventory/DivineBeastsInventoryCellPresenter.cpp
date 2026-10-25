#include "Screens/Inventory/DivineBeastsInventoryCellPresenter.h"
#include "Screens/Inventory/DivineBeastsInventoryPresentationCatalog.h"

#include "Blueprint/UserWidget.h"

FDivineBeastsInventoryCellDisplay
FDivineBeastsInventoryCellPresenter::MakeInventoryCell(
    const FGamePlatformInventorySlotViewModel* Slot,
    bool bIconReady, bool bReady, const FString& SelectedItemInstanceId)
{
    FDivineBeastsInventoryCellDisplay Cell;
    const bool bOccupied = Slot && Slot->bOccupied;
    Cell.bEnabled = Slot && bReady;
    Cell.BackgroundColor = bOccupied && Slot->Item.ItemInstanceId == SelectedItemInstanceId
        ? FLinearColor(0.34f, 0.55f, 0.40f, 1.0f)
        : FLinearColor(0.12f, 0.24f, 0.22f, 1.0f);
    Cell.bHasToolTip = true;
    Cell.ToolTip = bOccupied
        ? FDivineBeastsInventoryPresentationCatalog::Find(Slot->Item.ItemDefinitionId).DisplayName
        : NSLOCTEXT("DivineBeastsInventoryScreen", "InventoryEmptySlot", "空背包格");

    if (!Slot)
    {
        return Cell;
    }
    if (!bOccupied)
    {
        Cell.LabelText = FString::Printf(TEXT("%02d"), Slot->SlotIndex + 1);
        return Cell;
    }

    const FString DefinitionName = FDivineBeastsInventoryPresentationCatalog::Find(
        Slot->Item.ItemDefinitionId).DisplayName.ToString();
    Cell.LabelText = bIconReady
        ? FString::Printf(TEXT("×%d"), Slot->Item.Quantity)
        : DefinitionName.Left(7) + FString::Printf(TEXT("\n×%d"), Slot->Item.Quantity);
    if (Slot->Item.bPending)
    {
        Cell.LabelText += TEXT("…");
    }
    return Cell;
}

FDivineBeastsInventoryCellDisplay
FDivineBeastsInventoryCellPresenter::MakeQuickbarCell(
    const FGamePlatformInventoryQuickbarViewModel* Quick,
    int32 Index, bool bIconReady, bool bReady, int32 SelectedQuickbarIndex)
{
    FDivineBeastsInventoryCellDisplay Cell;
    // 项目快捷栏合同为12格；无效索引不产生可操作的表现。
    if (Index < 0 || Index >= 12)
    {
        return Cell;
    }
    Cell.LabelText = FString::FromInt(Index + 1);
    Cell.bEnabled = bReady;
    Cell.BackgroundColor = Index == SelectedQuickbarIndex
        ? FLinearColor(0.38f, 0.48f, 0.28f, 1.0f)
        : FLinearColor(0.14f, 0.24f, 0.23f, 1.0f);
    if (Quick && Quick->bAssigned)
    {
        Cell.LabelText = bIconReady
            ? FString::Printf(TEXT("×%d"), Quick->Item.Quantity)
            : FDivineBeastsInventoryPresentationCatalog::Find(
                Quick->Item.ItemDefinitionId).DisplayName.ToString().Left(7) +
                FString::Printf(TEXT("\n×%d"), Quick->Item.Quantity);
    }
    if (Quick && Quick->bPending)
    {
        Cell.LabelText += TEXT("…");
    }
    return Cell;
}

void FDivineBeastsInventoryCellPresenter::ApplyCell(
    UUserWidget& Screen, int32 Index, bool bQuickbar, bool bIconReady,
    const FDivineBeastsInventoryCellDisplay& Cell)
{
    if (Index < 0 || (bQuickbar && Index >= 12))
    {
        return;
    }
    // 命名控件的显示归平台槽位基类；项目仅提供图标几何差异和稳定作者命名。
    // 不更换既有Widget树与按钮事件，不新增第二套输入或加载执行器。
    FGamePlatformUISlotCellPresentation Presentation = Cell;
    Presentation.bLabelAtBottom = bIconReady;
    Presentation.LabelPadding = bIconReady
        ? FMargin(0.0f, bQuickbar ? 35.0f : 48.0f, 0.0f, 0.0f)
        : FMargin(0.0f);
    const FString TextName = bQuickbar
        ? FString::Printf(TEXT("QuickText_%d"), Index)
        : FString::Printf(TEXT("SlotText_%d"), Index);
    const FString ButtonName = bQuickbar
        ? FString::Printf(TEXT("QuickButton_%d"), Index)
        : FString::Printf(TEXT("SlotButton_%d"), Index);
    UGamePlatformSlotWidget::ApplyNamedSlotCellPresentation(
        Screen, FName(*TextName), FName(*ButtonName), Presentation);
}
