#include "Adapters/Notifications/DivineBeastsUINotificationLibrary.h"

namespace
{
FGamePlatformUINotificationRequest MakeRequest(
    FName Key,
    FName Channel,
    FName StyleId,
    FText Title,
    FText Message,
    int32 Priority,
    float LifetimeSeconds)
{
    FGamePlatformUINotificationRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.NotificationKey = Key;
    Request.Channel = Channel;
    Request.StyleId = StyleId;
    Request.Title = MoveTemp(Title);
    Request.Message = MoveTemp(Message);
    Request.Priority = Priority;
    Request.LifetimeSeconds =
        FMath::Max(0.0f, LifetimeSeconds);
    Request.bReplaceSameKey = true;
    return Request;
}
}

FGamePlatformUINotificationRequest
UDivineBeastsUINotificationLibrary::MakeToast(
    FName NotificationKey,
    FText Message,
    int32 Priority,
    float LifetimeSeconds)
{
    return MakeRequest(
        NotificationKey,
        TEXT("Toast"),
        TEXT("DBA.UI.Notification.Toast"),
        FText(),
        MoveTemp(Message),
        Priority,
        LifetimeSeconds);
}

FGamePlatformUINotificationRequest
UDivineBeastsUINotificationLibrary::MakeCenterMessage(
    FName NotificationKey,
    FText Message,
    int32 Priority,
    float LifetimeSeconds)
{
    return MakeRequest(
        NotificationKey,
        TEXT("CenterMessage"),
        TEXT("DBA.UI.Notification.CenterMessage"),
        FText(),
        MoveTemp(Message),
        Priority,
        LifetimeSeconds);
}

FGamePlatformUINotificationRequest
UDivineBeastsUINotificationLibrary::MakeRewardMessage(
    FName NotificationKey,
    FText Title,
    FText Message,
    int32 Priority,
    float LifetimeSeconds)
{
    return MakeRequest(
        NotificationKey,
        TEXT("Reward"),
        TEXT("DBA.UI.Notification.Reward"),
        MoveTemp(Title),
        MoveTemp(Message),
        Priority,
        LifetimeSeconds);
}
