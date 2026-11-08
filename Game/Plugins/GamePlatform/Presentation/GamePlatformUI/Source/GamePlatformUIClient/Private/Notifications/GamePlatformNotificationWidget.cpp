#include "Notifications/GamePlatformNotificationWidget.h"

void UGamePlatformNotificationWidget::ApplyNotificationRequest(
    const FGamePlatformUINotificationRequest& InRequest)
{
    NotificationRequest = InRequest;
    // 显式可拖动通知只让自己的标题/内容命中；普通通知继续完全穿透，不占用导航焦点。
    SetVisibility(WindowDrag.bEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
    BP_OnNotificationRequestApplied(NotificationRequest);
}
