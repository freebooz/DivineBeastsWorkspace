#include "Layers/GamePlatformUILayerStack.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Components/Overlay.h"
#include "Components/Widget.h"

UCommonActivatableWidgetStack* UGamePlatformUILayerStack::GetActivatableStack(
    EGamePlatformUILayer Layer) const
{
    switch (Layer)
    {
    case EGamePlatformUILayer::Screen:
        return ScreenLayer;
    case EGamePlatformUILayer::Modal:
        return ModalLayer;
    case EGamePlatformUILayer::System:
        return SystemLayer;
    case EGamePlatformUILayer::Loading:
        return LoadingLayer;
    case EGamePlatformUILayer::Debug:
        return DebugLayer;
    default:
        return nullptr;
    }
}

bool UGamePlatformUILayerStack::AddHUDWidget(UWidget* Widget)
{
    return IsValid(HUDLayer) && IsValid(Widget) && HUDLayer->AddChild(Widget) != nullptr;
}

bool UGamePlatformUILayerStack::AddNotificationWidget(UWidget* Widget)
{
    return IsValid(NotificationLayer) &&
           IsValid(Widget) &&
           NotificationLayer->AddChild(Widget) != nullptr;
}

void UGamePlatformUILayerStack::ClearHUD()
{
    if (IsValid(HUDLayer))
    {
        HUDLayer->ClearChildren();
    }
}

void UGamePlatformUILayerStack::ClearNotifications()
{
    if (IsValid(NotificationLayer))
    {
        NotificationLayer->ClearChildren();
    }
}
