#include "Services/GamePlatformNotificationService.h"

#include "Dialogs/GamePlatformToastWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "TimerManager.h"

namespace
{
constexpr int32 MaxActiveNotifications = 32;
}

void UGamePlatformNotificationService::Initialize(ULocalPlayer* InLocalPlayer)
{
    LocalPlayer = InLocalPlayer;
    ActiveNotifications.Reserve(8);
    ActiveByKey.Reserve(8);
}

void UGamePlatformNotificationService::SetRootLayout(
    UGamePlatformUILayerStack* InRootLayout)
{
    RootLayout = InRootLayout;
}

FGuid UGamePlatformNotificationService::SubmitNotification(
    FGamePlatformUINotificationRequest Request,
    TSubclassOf<UGamePlatformNotificationWidget> WidgetClass)
{
    if (!Request.IsValid() ||
        !WidgetClass ||
        WidgetClass->HasAnyClassFlags(CLASS_Abstract) ||
        !IsValid(LocalPlayer) ||
        !IsValid(RootLayout))
    {
        return FGuid();
    }

    APlayerController* PlayerController =
        LocalPlayer->GetPlayerController(GetServiceWorld());
    if (!IsValid(PlayerController))
    {
        return FGuid();
    }

    if (!Request.RequestId.IsValid())
    {
        Request.RequestId = FGuid::NewGuid();
    }

    // RequestId为实例身份，重复提交拒绝；NotificationKey才是允许优先级替换的业务合并键。
    if (ActiveNotifications.Contains(Request.RequestId)) return FGuid();
    UGamePlatformNotificationWidget* Widget =
        CreateWidget<UGamePlatformNotificationWidget>(
            PlayerController,
            WidgetClass);
    if (!IsValid(Widget) ||
        !AttachNotificationWidget(Widget, Request))
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
        }
        return FGuid();
    }

    return Request.RequestId;
}

bool UGamePlatformNotificationService::AttachToastWidget(
    UGamePlatformToastWidget* Widget)
{
    if (!IsValid(Widget))
    {
        return false;
    }

    FGamePlatformUINotificationRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.NotificationKey = Widget->ToastKey;
    Request.Channel = TEXT("Toast");
    Request.Priority = Widget->ToastPriority;
    Request.LifetimeSeconds = Widget->DurationSeconds;
    Request.bReplaceSameKey = true;
    return AttachNotificationWidget(Widget, Request);
}

bool UGamePlatformNotificationService::AttachNotificationWidget(
    UGamePlatformNotificationWidget* Widget,
    const FGamePlatformUINotificationRequest& Request)
{
    if (!IsValid(Widget) ||
        !IsValid(RootLayout) ||
        !Request.RequestId.IsValid())
    {
        return false;
    }

    if (ActiveNotifications.Contains(Request.RequestId)) return false;

    if (!Request.NotificationKey.IsNone())
    {
        if (const FGuid* ExistingId = ActiveByKey.Find(Request.NotificationKey))
        {
            const FActiveNotification* Existing =
                ActiveNotifications.Find(*ExistingId);
            if (Existing &&
                (!Request.bReplaceSameKey ||
                 Request.Priority <= Existing->Priority))
            {
                return false;
            }

            DismissNotification(*ExistingId);
        }
    }

    if (ActiveNotifications.Num() >= MaxActiveNotifications)
    {
        return false;
    }

    Widget->ApplyNotificationRequest(Request);
    if (!RootLayout->AddNotificationWidget(Widget))
    {
        return false;
    }

    FActiveNotification Entry;
    Entry.Widget = Widget;
    Entry.NotificationKey = Request.NotificationKey;
    Entry.Priority = Request.Priority;

    if (Request.LifetimeSeconds > KINDA_SMALL_NUMBER)
    {
        if (UWorld* World = GetServiceWorld())
        {
            const TWeakObjectPtr<UGamePlatformNotificationService> WeakThis(this);
            const FGuid RequestId = Request.RequestId;
            World->GetTimerManager().SetTimer(
                Entry.TimerHandle,
                FTimerDelegate::CreateLambda([WeakThis, RequestId]()
                {
                    if (UGamePlatformNotificationService* Self = WeakThis.Get())
                    {
                        Self->DismissNotification(RequestId);
                    }
                }),
                Request.LifetimeSeconds,
                false);
        }
    }

    ActiveNotifications.Add(Request.RequestId, Entry);
    if (!Request.NotificationKey.IsNone())
    {
        ActiveByKey.Add(Request.NotificationKey, Request.RequestId);
    }
    return true;
}

bool UGamePlatformNotificationService::DismissNotification(FGuid RequestId)
{
    FActiveNotification* Entry = ActiveNotifications.Find(RequestId);
    if (!Entry)
    {
        return false;
    }

    if (UWorld* World = GetServiceWorld())
    {
        World->GetTimerManager().ClearTimer(Entry->TimerHandle);
    }

    if (UGamePlatformNotificationWidget* Widget = Entry->Widget.Get())
    {
        Widget->RemoveFromParent();
    }

    if (!Entry->NotificationKey.IsNone())
    {
        if (const FGuid* Current = ActiveByKey.Find(Entry->NotificationKey);
            Current && *Current == RequestId)
        {
            ActiveByKey.Remove(Entry->NotificationKey);
        }
    }

    ActiveNotifications.Remove(RequestId);
    return true;
}

void UGamePlatformNotificationService::Clear()
{
    TArray<FGuid> RequestIds;
    ActiveNotifications.GetKeys(RequestIds);
    for (const FGuid& RequestId : RequestIds)
    {
        DismissNotification(RequestId);
    }
    ActiveNotifications.Reset();
    ActiveByKey.Reset();
}

UWorld* UGamePlatformNotificationService::GetServiceWorld() const
{
    return IsValid(LocalPlayer) ? LocalPlayer->GetWorld() : nullptr;
}
