#pragma once

#include "CommonUserWidget.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUILayerStack.generated.h"

class UCommonActivatableWidgetStack;
class UOverlay;
class UWidget;

/**
 * 单个 LocalPlayer（本地玩家）的统一 UI 层容器。
 * 页面栈使用 CommonUI；HUD/Notification 为非激活普通层。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformUILayerStack : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UCommonActivatableWidgetStack* GetActivatableStack(EGamePlatformUILayer Layer) const;

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddHUDWidget(UWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddNotificationWidget(UWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearHUD();

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearNotifications();

protected:
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UOverlay> HUDLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> ScreenLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> ModalLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> SystemLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UOverlay> NotificationLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> LoadingLayer = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> DebugLayer = nullptr;
};
