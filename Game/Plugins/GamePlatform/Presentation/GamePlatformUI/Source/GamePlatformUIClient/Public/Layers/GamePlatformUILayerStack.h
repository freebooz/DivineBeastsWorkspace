#pragma once

#include "Core/GamePlatformWidgetBase.h"
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
class GAMEPLATFORMUICLIENT_API UGamePlatformUILayerStack
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()

public:
    UCommonActivatableWidgetStack* GetActivatableStack(EGamePlatformUILayer Layer) const;

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddHUDWidget(UWidget* Widget);

    /** 添加名称板、世界血条或世界标记到集中投影层。 */
    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddWorldProjectionWidget(UWidget* Widget);

    /** 添加伤害飘字、命中、拾取等短生命周期反馈到反馈层。 */
    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddFeedbackWidget(UWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    bool AddNotificationWidget(UWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearHUD();

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearWorldProjection();

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearFeedback();

    UFUNCTION(BlueprintCallable, Category="UI|Layer")
    void ClearNotifications();

protected:
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UOverlay> HUDLayer = nullptr;

    /** 世界空间UI统一投影层；避免名称板和Marker混入常驻HUD。 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UOverlay> WorldProjectionLayer = nullptr;

    /** 高频即时反馈层；与通知层分离，避免伤害飘字占用通知队列。 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UOverlay> FeedbackLayer = nullptr;

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
