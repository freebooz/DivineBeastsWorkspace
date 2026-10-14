// 平台客户端槽位原子：消费领域只读快照并发布中立交互意图，不解释英雄、技能或背包规则。
// 由本地页面在游戏线程调用；仅拥有本Widget显示状态及订阅，失活销毁时撤销，不发送网络请求。
#include "Components/GamePlatformSlotWidget.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

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
