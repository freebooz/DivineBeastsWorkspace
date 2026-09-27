#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIManagerSubsystem.generated.h"

struct FStreamableHandle;
class UGamePlatformHUDWidget;
class UGamePlatformLoadingScreenService;
class UGamePlatformToastWidget;
class UGamePlatformUILayerStack;
class UGamePlatformUIRouteDefinition;
class UGamePlatformUIScreen;
class UGamePlatformUIScreenDefinition;
class UGamePlatformViewModelBase;
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

    /** 在ClientTravel/LoadMap前清理World-scoped页面与过期异步请求。 */
    UFUNCTION(BlueprintCallable, Category="UI|Manager")
    void PrepareForTravel();

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformUILayerStack* GetRootLayout() const { return RootLayout; }

    UFUNCTION(BlueprintPure, Category="UI|Manager")
    UGamePlatformLoadingScreenService* GetLoadingScreenService() const { return LoadingScreenService; }

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
    TMap<TWeakObjectPtr<UGamePlatformUIScreen>, TSharedPtr<FStreamableHandle>> ActiveScreenLeases;
    TSet<TWeakObjectPtr<UGamePlatformUIScreen>> PauseScreens;
    TSet<TWeakObjectPtr<UGamePlatformUIScreen>> TravelPersistentScreens;
    TMap<FName, TWeakObjectPtr<UGamePlatformToastWidget>> ActiveToasts;
    TMap<FName, FTimerHandle> ToastTimers;
    FDelegateHandle PreLoadMapHandle;

    int32 NextGeneration = 1;
    bool bPauseAppliedByUI = false;

    bool IsDefinitionAllowed(const UGamePlatformUIScreenDefinition& Definition) const;
    bool IsRouteTargetValid(const UGamePlatformUIRouteDefinition& Definition) const;
    TSoftClassPtr<UGamePlatformUIScreen> ResolveWidgetClass(
        const UGamePlatformUIScreenDefinition& Definition) const;

    bool HasAnyRouteCycle() const;
    void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
    void HandleScreenAssetsLoaded(FGuid RequestId);
    void HandleScreenDeactivated(UGamePlatformUIScreen* Screen);
    void FailRequest(FGuid RequestId, FName ScreenId, const FText& Reason);
    void CleanupPendingRequest(FGuid RequestId, bool bCancelLoad);
    void ExpireToast(FName ToastKey, TWeakObjectPtr<UGamePlatformToastWidget> ExpectedWidget);
    void RefreshStandalonePause();
};
