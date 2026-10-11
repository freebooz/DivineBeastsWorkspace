// 平台客户端只读资源快照展示：调用方负责权威、单位及事件，本实现只更新既有UMG，不轮询或加载项目资产。
#include "Components/GamePlatformResourceBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UGamePlatformResourceBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshResourceValueText();
}

void UGamePlatformResourceBarWidget::RefreshResourceValueText()
{
    // 标准可选控件名属于中立展示合同；旧蓝图没有该文本时保持其自定义表现，不增加必需绑定。
    UTextBlock* ValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResourceValueText")));
    if (!IsValid(ValueText)) return;
    const bool bHasDisplayValue = ResourceState.bShowValueText &&
        FMath::IsFinite(ResourceState.CurrentValue) && FMath::IsFinite(ResourceState.MaximumValue) &&
        ResourceState.MaximumValue > KINDA_SMALL_NUMBER;
    ValueText->SetVisibility(bHasDisplayValue ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bHasDisplayValue) { ValueText->SetText(FText::GetEmpty()); return; }
    // 数字来自快照，整数不补小数，最多六位小数保留小单位资源；不改真实值或字体。
    FNumberFormattingOptions Format;
    Format.SetUseGrouping(false).SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(6);
    ValueText->SetText(FText::Format(NSLOCTEXT("GamePlatformUI", "ResourceCurrentMaximum", "{0} / {1}"),
        FText::AsNumber(ResourceState.CurrentValue, &Format), FText::AsNumber(ResourceState.MaximumValue, &Format)));
}

void UGamePlatformResourceBarWidget::ApplyResourceState(
    const FGamePlatformUIResourceBarState& InState)
{
    const bool bEquivalent =
        ResourceState.ResourceId == InState.ResourceId &&
        ResourceState.bShowValueText == InState.bShowValueText &&
        // 近似相等会漏掉数值文本或未知/已知阈值跨越；完全相同快照才跳过事件。
        ResourceState.CurrentValue == InState.CurrentValue &&
        ResourceState.MaximumValue == InState.MaximumValue;

    if (bEquivalent)
    {
        return;
    }

    ResourceState = InState;
    RefreshResourceValueText();

    if (IsValid(ProgressBar))
    {
        ProgressBar->SetPercent(
            static_cast<float>(ResourceState.GetNormalizedValue()));
    }

    BP_OnResourceStateChanged(ResourceState);
}
