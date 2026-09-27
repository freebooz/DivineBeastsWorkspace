#include "Notifications/GamePlatformNotificationWidget.h"

void UGamePlatformNotificationWidget::ApplyNotificationRequest(
    const FGamePlatformUINotificationRequest& InRequest)
{
    NotificationRequest = InRequest;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    BP_OnNotificationRequestApplied(NotificationRequest);
}
