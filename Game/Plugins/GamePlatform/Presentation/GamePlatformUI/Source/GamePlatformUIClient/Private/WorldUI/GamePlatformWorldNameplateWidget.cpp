// 平台客户端名称板：只显示调用者事件快照；没有项目资源、GAS、网络或Tick依赖。
#include "WorldUI/GamePlatformWorldNameplateWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UGamePlatformWorldNameplateWidget::ApplyWorldUIRequest(const FGamePlatformWorldUIRequest& InRequest)
{
    const uint64 Generation = ++VisualApplyGeneration;
    if (!bDefaultColorsCaptured)
    {
        if (auto* Fill = Cast<UProgressBar>(GetWidgetFromName(TEXT("ResourceFill")))) DefaultResourceColor = Fill->GetFillColorAndOpacity();
        if (auto* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText")))) DefaultNameColor = Name->GetColorAndOpacity();
        bDefaultColorsCaptured = true;
    }
    Super::ApplyWorldUIRequest(InRequest);
    // 蓝图事件可能同步回收本控件；只投影仍属于本次注册的快照。
    if (Generation != VisualApplyGeneration || GetWorldUIRequest().RequestId != InRequest.RequestId) return;
    auto* Fill = Cast<UProgressBar>(GetWidgetFromName(TEXT("ResourceFill")));
    auto* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText")));
    if (Fill)
    {
        Fill->SetPercent(static_cast<float>(InRequest.Resource.GetNormalizedValue()));
        // 无资源的交互目标仍可显示选中名称/标记；不能用0/0虚构生命条。
        Fill->SetVisibility(InRequest.Resource.MaximumValue > 0. ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        Fill->SetFillColorAndOpacity(InRequest.bSelected ? InRequest.SelectionTint : DefaultResourceColor);
    }
    if (Name)
    {
        // 项目可选单行提示：名称本身保留非颜色选中符号，不再让第二行标记遮住远处名称板。
        Name->SetText(InRequest.bSelected && InRequest.bCompactSelection
            ? FText::Format(NSLOCTEXT("GamePlatformWorldUI", "CompactSelectedName", "▶ {0}"), InRequest.PrimaryText)
            : InRequest.PrimaryText);
        Name->SetColorAndOpacity(InRequest.bSelected ? FSlateColor(InRequest.SelectionTint) : DefaultNameColor);
    }
    if (auto* Value = Cast<UTextBlock>(GetWidgetFromName(TEXT("ValueText"))))
        Value->SetText(InRequest.SecondaryText);
    if (auto* Outline = Cast<UImage>(GetWidgetFromName(TEXT("SelectionOutline"))))
    {
        Outline->SetColorAndOpacity(InRequest.SelectionTint);
        // 紧凑模式用于怪物血条：保留脚下选中圈及名称箭头，不绘制血条边框。
        Outline->SetVisibility(InRequest.bSelected && !InRequest.bCompactSelection
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (auto* Marker = Cast<UTextBlock>(GetWidgetFromName(TEXT("SelectionMarker"))))
    {
        // 装饰文案可由项目本地化；未提供时仍有箭头形状，保证状态不只依赖红/绿识别。
        Marker->SetText(InRequest.SelectionText.IsEmpty() ? FText::FromString(TEXT("▶  ◀")) : InRequest.SelectionText);
        Marker->SetColorAndOpacity(InRequest.SelectionTint);
        Marker->SetVisibility(InRequest.bSelected && !InRequest.bCompactSelection
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
