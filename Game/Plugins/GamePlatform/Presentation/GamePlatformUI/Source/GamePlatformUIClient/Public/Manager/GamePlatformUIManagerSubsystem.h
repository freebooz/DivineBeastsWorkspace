#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "GamePlatformUITypes.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "Requests/GamePlatformUINotificationRequest.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "GamePlatformUIManagerSubsystem.generated.h"

struct FStreamableHandle;
class UGamePlatformHUDWidget;
class UGamePlatformFeedbackService;
class UGamePlatformFeedbackWidget;
class UGamePlatformNotificationService;
class UGamePlatformNotificationWidget;
class UGamePlatformLoadingScreenService;
class UGamePlatformToastWidget;
class UGamePlatformUILayerStack;
class UGamePlatformUIRouteDefinition;
class UGamePlatformUIScreen;
class UGamePlatformUIScreenDefinition;
class UGamePlatformViewModelBase;
class UGamePlatformWorldUIService;
class UGamePlatformWorldWidgetBase;
struct FWorldContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGamePlatformUIScreenOpened,
    FGuid, RequestId,
    FName, ScreenId,
    UGamePlatformUIScreen*, Screen);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGamePlatformUIScreenOpenFailed,
    FGuid, RequestId,
    FName, ScreenId,
    FText, Reason);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUIScreenClosed,
    FName, ScreenId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUIAccessibilityPreferencesChanged,
    FGamePlatformUIAccessibilityPreferences, Preferences);

/**
 * 每个 LocalPlayer 唯一的平台 UI 门面。
 * 项目业务只注册 Definition/Route、提交页面意图，不直接 AddToViewport。
 */
UCLASS()
class GAMEPLATFORMUICLIENT_API UGamePlatformUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool InstallRootLayoutClass(TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool RegisterScreenDefinition(UGamePlatformUIScreenDefinition* Definition);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool UnregisterScreenDefinition(FName ScreenId);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool RegisterRouteDefinition(UGamePlatformUIRouteDefinition* Definition);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool UnregisterRouteDefinition(FName RouteId);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    void SetContextTags(const FGameplayTagContainer& InContextTags);

    /** 更新跨游戏可访问性偏好；只控制UI体验，不改变Gameplay权威状态。 */
    UFUNCTION(BlueprintCallable, Category="UI|Accessibility")
    void SetAccessibilityPreferences(FGamePlatformUIAccessibilityPreferences InPreferences);

    UFUNCTION(BlueprintPure, Category="UI|Accessibility")
    FGamePlatformUIAccessibilityPreferences GetAccessibilityPreferences() const
    {
        return AccessibilityPreferences;
    }

    /** 在减少动态效果开启时，将默认动画策略降级为即时切换。 */
    UFUNCTION(BlueprintPure, Category="UI|Accessibility")
    EGamePlatformUITransition ResolveTransition(EGamePlatformUITransition RequestedTransition) const;

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    FGamePlatformUIAsyncRequest OpenScreenAsync(
        FName ScreenId,
        UGamePlatformViewModelBase* ViewModel);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    FGamePlatformUIAsyncRequest OpenRouteAsync(
        FName RouteId,
        UGamePlatformViewModelBase* ViewModel);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool CancelOpen(FGuid RequestId);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool CloseScreen(UGamePlatformUIScreen* Screen);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool AttachHUDWidget(UGamePlatformHUDWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    bool AttachToastWidget(UGamePlatformToastWidget* Widget);

    /** 通过平台通知服务提交请求；业务模块不直接操作NotificationLayer。 */
    UFUNCTION(BlueprintCallable, Category="UI|Notification")
    FGuid SubmitNotification(
        FGamePlatformUINotificationRequest Request,
        TSubclassOf<UGamePlatformNotificationWidget> WidgetClass);

    /** 提交短生命周期高频视觉反馈；服务负责合并、限流和对象池复用。 */
    UFUNCTION(BlueprintCallable, Category="UI|Feedback")
    FGuid SubmitFeedback(
        FGamePlatformUIFeedbackRequest Request,
        TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass);

    /** 注册需要持续世界坐标投影的名称板/Marker等UI。 */
    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    FGuid RegisterWorldUI(
        FGamePlatformWorldUIRequest Request,
        TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass);

    /** 更新已注册世界UI的轻量只读数据与世界位置。 */
    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UpdateWorldUI(
        FGuid RequestId,
        FGamePlatformWorldUIRequest Request);

    /** 注销世界UI并回收对应Widget。 */
    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UnregisterWorldUI(FGuid RequestId);

    /** 在ClientTravel/LoadMap前清理World-scoped页面与过期异步请求。 */
    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    void PrepareForTravel();

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformUILayerStack* GetRootLayout() const { return RootLayout; }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformLoadingScreenService* GetLoadingScreenService() const { return LoadingScreenService; }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformNotificationService* GetNotificationService() const
    {
        return NotificationService;
    }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformFeedbackService* GetFeedbackService() const
    {
        return FeedbackService;
    }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformWorldUIService* GetWorldUIService() const
    {
        return WorldUIService;
    }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    bool HasScreenDefinition(FName ScreenId) const;

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    bool HasRouteDefinition(FName RouteId) const;

    /** 读取已注册Route的显式BackRoute；不包含项目业务流程。 */
    UFUNCTION(BlueprintPure, Category="UI|Manager")
    FName GetBackRouteId(FName RouteId) const;

    UPROPERTY(BlueprintAssignable, Category="UI|Manager")
    FGamePlatformUIScreenOpened OnScreenOpened;

    UPROPERTY(BlueprintAssignable, Category="UI|Manager")
    FGamePlatformUIScreenOpenFailed OnScreenOpenFailed;

    UPROPERTY(BlueprintAssignable, Category="UI|Manager")
    FGamePlatformUIScreenClosed OnScreenClosed;

    UPROPERTY(BlueprintAssignable, Category="UI|Accessibility")
    FGamePlatformUIAccessibilityPreferencesChanged OnAccessibilityPreferencesChanged;

private:
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformLoadingScreenService> LoadingScreenService = nullptr;

    /** 低频屏幕通知服务；由UIManager统一持有，不增加额外Subsystem。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformNotificationService> NotificationService = nullptr;

    /** 高频反馈对象池服务。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformFeedbackService> FeedbackService = nullptr;

    /** 世界空间UI集中投影服务。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformWorldUIService> WorldUIService = nullptr;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformUIScreenDefinition>> ScreenDefinitions;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformUIRouteDefinition>> RouteDefinitions;

    UPROPERTY(Transient)
    TMap<FGuid, FGamePlatformUIAsyncRequest> PendingRequests;

    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UGamePlatformViewModelBase>> PendingViewModels;

    UPROPERTY(Transient)
    FGameplayTagContainer ContextTags;

    UPROPERTY(Transient)
    FGamePlatformUIAccessibilityPreferences AccessibilityPreferences;

    TMap<FGuid, TSharedPtr<FStreamableHandle>> PendingLoads;
    /** 移动/其他平台专属Widget加载失败时，只允许针对同一请求回退默认类一次。 */
    TSet<FGuid> PendingDefaultWidgetRetries;
    TMap<TWeakObjectPtr<UGamePlatformUIScreen>, TSharedPtr<FStreamableHandle>> ActiveScreenLeases;
    TSet<TWeakObjectPtr<UGamePlatformUIScreen>> PauseScreens;
    TSet<TWeakObjectPtr<UGamePlatformUIScreen>> TravelPersistentScreens;
    FDelegateHandle PreLoadMapHandle;

    int32 NextGeneration = 1;
    bool bPauseAppliedByUI = false;

    bool IsDefinitionAllowed(const UGamePlatformUIScreenDefinition& Definition) const;
    bool IsRouteTargetValid(const UGamePlatformUIRouteDefinition& Definition) const;
    /** 默认先解析平台变体，重试阶段强制使用共享Widget类以终止失败循环。 */
    TSoftClassPtr<UGamePlatformUIScreen> ResolveWidgetClass(
        const UGamePlatformUIScreenDefinition& Definition,
        bool bUseDefaultWidget = false) const;

    bool HasAnyRouteCycle() const;
    void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
    void HandleScreenAssetsLoaded(FGuid RequestId);
    void HandleScreenDeactivated(UGamePlatformUIScreen* Screen);
    void FailRequest(FGuid RequestId, FName ScreenId, const FText& Reason);
    void CleanupPendingRequest(FGuid RequestId, bool bCancelLoad);
    void RefreshStandalonePause();
};
