// 平台客户端槽位原子：消费领域只读快照并发布中立交互意图，不解释英雄、技能或背包规则。
// 由本地页面在游戏线程调用；仅拥有本Widget显示状态及订阅，失活销毁时撤销，不发送网络请求。
#include "Components/GamePlatformSlotWidget.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

bool UGamePlatformSlotWidget::AreSlotStatesEquivalent(
    const FGamePlatformUISlotState& A, const FGamePlatformUISlotState& B,
    bool bCompareCount)
{
    return A.SlotId == B.SlotId && A.ContentId == B.ContentId &&
        A.Icon == B.Icon && (!bCompareCount || A.Count == B.Count) &&
        FMath::IsNearlyEqual(A.OverlayProgress, B.OverlayProgress, 0.0001f) &&
        A.bEnabled == B.bEnabled && A.bPending == B.bPending;
}

void UGamePlatformSlotWidget::ApplyNamedSlotCellPresentation(
    UUserWidget& Root, FName LabelWidgetName, FName ButtonWidgetName,
    const FGamePlatformUISlotCellPresentation& Presentation)
{
    check(IsInGameThread());
    if (UTextBlock* Label = Cast<UTextBlock>(Root.GetWidgetFromName(LabelWidgetName)))
    {
        Label->SetText(FText::FromString(Presentation.LabelText));
        if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Label->Slot))
        {
            ButtonSlot->SetHorizontalAlignment(HAlign_Center);
            ButtonSlot->SetVerticalAlignment(
                Presentation.bLabelAtBottom ? VAlign_Bottom : VAlign_Center);
            ButtonSlot->SetPadding(Presentation.LabelPadding);
        }
    }
    if (UButton* Button = Cast<UButton>(Root.GetWidgetFromName(ButtonWidgetName)))
    {
        Button->SetIsEnabled(Presentation.bEnabled);
        Button->SetBackgroundColor(Presentation.BackgroundColor);
        if (Presentation.bHasToolTip)
        {
            Button->SetToolTipText(Presentation.ToolTip);
        }
    }
}

void UGamePlatformSlotWidget::ApplyOverlayTiming(float EndsAtAppSeconds, float DurationSeconds)
{
    // 旧整块覆盖只作兼容命名保留。遮罩的角度由MD_UI材质时钟连续推进，不轮询领域模型。
    if (UWidget* Legacy = GetWidgetFromName(TEXT("CooldownOverlay")))
        Legacy->SetVisibility(ESlateVisibility::Collapsed);
    UImage* Sweep = Cast<UImage>(GetWidgetFromName(TEXT("CooldownSweep")));
    if (!Sweep) return; // 背包等没有旋转覆盖的既有槽位不需要项目资产。
    const bool bValidTiming = FMath::IsFinite(EndsAtAppSeconds) && FMath::IsFinite(DurationSeconds) &&
        EndsAtAppSeconds > 0.f && DurationSeconds > 0.f && !SlotState.ContentId.IsNone();
    if (UMaterialInstanceDynamic* Material = Sweep->GetDynamicMaterial())
    {
        Material->SetScalarParameterValue(TEXT("CooldownEndsAt"), bValidTiming ? EndsAtAppSeconds : 0.f);
        Material->SetScalarParameterValue(TEXT("CooldownDuration"), bValidTiming ? DurationSeconds : 1.f);
    }
    Sweep->SetVisibility(bValidTiming ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UGamePlatformSlotWidget::RequestSlotInvocation()
{
    if (SlotState.SlotId.IsNone() || SlotState.ContentId.IsNone() ||
        !SlotState.bEnabled || SlotState.bPending || !SlotInvoked.IsBound()) { return false; }
    // 消费者可能同步换页或销毁Widget。只发布值拷贝，不在回调返回后写入旧视图或业务状态。
    const FName InvokedSlotId = SlotState.SlotId;
    SlotInvoked.Broadcast(InvokedSlotId);
    return true;
}

FReply UGamePlatformSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::LeftMouseButton && RequestSlotInvocation())
        return FReply::Handled();
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UGamePlatformSlotWidget::NativeDestruct()
{
    ApplyOverlayTiming(0.f, 0.f);
    SlotInvoked.Clear();
    Super::NativeDestruct();
}

void UGamePlatformSlotWidget::ApplySlotState(
    const FGamePlatformUISlotState& InState)
{
    FGamePlatformUISlotState ClampedState = InState;
    ClampedState.OverlayProgress = FMath::Clamp(InState.OverlayProgress, 0.0f, 1.0f);
    if (AreSlotStatesEquivalent(SlotState, ClampedState))
    {
        return;
    }
    SlotState = MoveTemp(ClampedState);
    BP_OnSlotStateChanged(SlotState);
}
