// 平台客户端槽位原子：消费领域只读快照并发布中立交互意图，不解释英雄、技能或背包规则。
// 由本地页面在游戏线程调用；仅拥有本Widget显示状态及订阅，失活销毁时撤销，不发送网络请求。
#include "Components/GamePlatformSlotWidget.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Materials/MaterialInstanceDynamic.h"

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
    const float ClampedOverlay =
        FMath::Clamp(InState.OverlayProgress, 0.0f, 1.0f);

    if (SlotState.SlotId == InState.SlotId &&
        SlotState.ContentId == InState.ContentId &&
        SlotState.Icon == InState.Icon &&
        SlotState.Count == InState.Count &&
        FMath::IsNearlyEqual(
            SlotState.OverlayProgress,
            ClampedOverlay,
            0.0001f) &&
        SlotState.bEnabled == InState.bEnabled &&
        SlotState.bPending == InState.bPending)
    {
        return;
    }

    SlotState = InState;
    SlotState.OverlayProgress = ClampedOverlay;
    BP_OnSlotStateChanged(SlotState);
}
