#include "Manager/GamePlatformUIManagerSubsystem.h"

#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Dialogs/GamePlatformToastWidget.h"
#include "HAL/PlatformProperties.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Routing/GamePlatformUIRouteDefinition.h"
#include "Screens/GamePlatformHUDWidget.h"
#include "Screens/GamePlatformUIScreen.h"
#include "ViewModels/GamePlatformViewModelBase.h"

#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#define LOCTEXT_NAMESPACE "GamePlatformUI"

namespace
{
constexpr int32 MaxScreenDefinitions = 256;
constexpr int32 MaxRouteDefinitions = 512;
constexpr int32 MaxPendingOpenRequests = 32;
constexpr int32 MaxActiveToasts = 32;
constexpr int32 MaxPreloadAssetsPerScreen = 32;
}

void UGamePlatformUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadingScreenService = NewObject<UGamePlatformLoadingScreenService>(this);
    ScreenDefinitions.Reserve(64);
    RouteDefinitions.Reserve(64);
    PendingRequests.Reserve(8);
    PendingViewModels.Reserve(8);
    PendingLoads.Reserve(8);
    ActiveToasts.Reserve(8);
    ToastTimers.Reserve(8);
    PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
        this,
        &UGamePlatformUIManagerSubsystem::HandlePreLoadMap);
}

void UGamePlatformUIManagerSubsystem::Deinitialize()
{
    if (PreLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
        PreLoadMapHandle.Reset();
    }

    if (UWorld* World = GetWorld())
    {
        for (TPair<FName, FTimerHandle>& Pair : ToastTimers)
        {
            World->GetTimerManager().ClearTimer(Pair.Value);
        }
    }
    ToastTimers.Reset();
    ActiveToasts.Reset();
    TArray<FGuid> RequestIds;
    PendingRequests.GetKeys(RequestIds);
    for (const FGuid& RequestId : RequestIds)
    {
        CleanupPendingRequest(RequestId, true);
    }

    if (IsValid(LoadingScreenService))
    {
        LoadingScreenService->ReleaseAll();
    }

    TArray<TWeakObjectPtr<UGamePlatformUIScreen>> ActiveScreens;
    ActiveScreenLeases.GetKeys(ActiveScreens);
    for (const TWeakObjectPtr<UGamePlatformUIScreen>& ScreenPtr : ActiveScreens)
    {
        if (UGamePlatformUIScreen* Screen = ScreenPtr.Get())
        {
            Screen->OnPlatformDeactivated().RemoveAll(this);
            Screen->DeactivateWidget();
        }
    }

    ActiveScreenLeases.Reset();
    PauseScreens.Reset();

    if (IsValid(RootLayout))
    {
        RootLayout->RemoveFromParent();
        RootLayout = nullptr;
    }

    if (bPauseAppliedByUI)
    {
        if (APlayerController* PlayerController = GetLocalPlayer()->GetPlayerController(GetWorld()))
        {
            PlayerController->SetPause(false);
        }
        bPauseAppliedByUI = false;
    }

    ScreenDefinitions.Reset();
    RouteDefinitions.Reset();
    Super::Deinitialize();
}

void UGamePlatformUIManagerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    RefreshStandalonePause();
}

bool UGamePlatformUIManagerSubsystem::InstallRootLayoutClass(
    TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass)
{
    if (!RootLayoutClass ||
        RootLayoutClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }

    APlayerController* PlayerController = GetLocalPlayer()->GetPlayerController(GetWorld());
    if (!IsValid(PlayerController))
    {
        return false;
    }

    UGamePlatformUILayerStack* NewRoot =
        CreateWidget<UGamePlatformUILayerStack>(PlayerController, RootLayoutClass);
    if (!IsValid(NewRoot))
    {
        return false;
    }

    if (IsValid(RootLayout))
    {
        RootLayout->RemoveFromParent();
    }

    RootLayout = NewRoot;
    return RootLayout->AddToPlayerScreen(0);
}

bool UGamePlatformUIManagerSubsystem::RegisterScreenDefinition(
    UGamePlatformUIScreenDefinition* Definition)
{
    FText ValidationError;
    if (!IsValid(Definition) ||
        !Definition->ValidateDefinition(ValidationError) ||
        ScreenDefinitions.Num() >= MaxScreenDefinitions ||
        ScreenDefinitions.Contains(Definition->ScreenId))
    {
        return false;
    }

    ScreenDefinitions.Add(Definition->ScreenId, Definition);
    return true;
}

bool UGamePlatformUIManagerSubsystem::UnregisterScreenDefinition(FName ScreenId)
{
    if (ScreenId.IsNone())
    {
        return false;
    }

    for (const TPair<FName, TObjectPtr<UGamePlatformUIRouteDefinition>>& Pair :
         RouteDefinitions)
    {
        if (IsValid(Pair.Value) && Pair.Value->ScreenId == ScreenId)
        {
            return false;
        }
    }

    return ScreenDefinitions.Remove(ScreenId) > 0;
}

bool UGamePlatformUIManagerSubsystem::RegisterRouteDefinition(
    UGamePlatformUIRouteDefinition* Definition)
{
    FText ValidationError;
    if (!IsValid(Definition) ||
        !Definition->ValidateDefinition(ValidationError) ||
        !IsRouteTargetValid(*Definition) ||
        RouteDefinitions.Num() >= MaxRouteDefinitions ||
        RouteDefinitions.Contains(Definition->RouteId))
    {
        return false;
    }

    RouteDefinitions.Add(Definition->RouteId, Definition);
    if (HasAnyRouteCycle())
    {
        RouteDefinitions.Remove(Definition->RouteId);
        return false;
    }

    return true;
}

bool UGamePlatformUIManagerSubsystem::UnregisterRouteDefinition(FName RouteId)
{
    if (RouteId.IsNone())
    {
        return false;
    }

    // 与 Screen 注销保持相同的 Fail Closed（失败关闭）策略：
    // 仍被其它 Route 的 fallback/back 引用时不得删除，避免留下悬空路由。
    for (const TPair<FName, TObjectPtr<UGamePlatformUIRouteDefinition>>& Pair :
         RouteDefinitions)
    {
        if (Pair.Key == RouteId || !IsValid(Pair.Value))
        {
            continue;
        }

        if (Pair.Value->FallbackRouteId == RouteId ||
            Pair.Value->BackRouteId == RouteId)
        {
            return false;
        }
    }

    return RouteDefinitions.Remove(RouteId) > 0;
}

void UGamePlatformUIManagerSubsystem::SetContextTags(
    const FGameplayTagContainer& InContextTags)
{
    ContextTags = InContextTags;
}

void UGamePlatformUIManagerSubsystem::SetAccessibilityPreferences(
    FGamePlatformUIAccessibilityPreferences InPreferences)
{
    // 平台只做安全下限归一化；最终字号/触控尺寸仍由具体Style和布局决定。
    InPreferences.TextScale = FMath::Max(0.5f, InPreferences.TextScale);
    InPreferences.TouchTargetScale = FMath::Max(1.0f, InPreferences.TouchTargetScale);
    AccessibilityPreferences = InPreferences;
    OnAccessibilityPreferencesChanged.Broadcast(AccessibilityPreferences);
}

EGamePlatformUITransition UGamePlatformUIManagerSubsystem::ResolveTransition(
    EGamePlatformUITransition RequestedTransition) const
{
    if (AccessibilityPreferences.bReducedMotion &&
        RequestedTransition == EGamePlatformUITransition::Default)
    {
        return EGamePlatformUITransition::Instant;
    }

    return RequestedTransition;
}

FGamePlatformUIAsyncRequest UGamePlatformUIManagerSubsystem::OpenScreenAsync(
    FName ScreenId,
    UGamePlatformViewModelBase* ViewModel)
{
    FGamePlatformUIAsyncRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.ScreenId = ScreenId;
    Request.Generation = NextGeneration++;

    if (PendingRequests.Num() >= MaxPendingOpenRequests)
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("OpenBackpressure", "并发页面打开请求过多，请稍后重试。"));
        return Request;
    }

    TObjectPtr<UGamePlatformUIScreenDefinition>* DefinitionPtr = ScreenDefinitions.Find(ScreenId);
    UGamePlatformUIScreenDefinition* Definition =
        DefinitionPtr ? DefinitionPtr->Get() : nullptr;

    if (!IsValid(Definition))
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("UnknownScreen", "页面未注册。"));
        return Request;
    }

    FText DefinitionError;
    if (!Definition->ValidateDefinition(DefinitionError))
    {
        FailRequest(Request.RequestId, ScreenId, DefinitionError);
        return Request;
    }

    if (!IsValid(RootLayout))
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("MissingRoot", "UI根布局尚未安装。"));
        return Request;
    }

    if (!IsDefinitionAllowed(*Definition))
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("ScreenBlocked", "当前状态不允许打开该页面。"));
        return Request;
    }

    UCommonActivatableWidgetStack* Stack =
        RootLayout->GetActivatableStack(Definition->Layer);
    if (!IsValid(Stack))
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("MissingLayer", "页面目标层未配置。"));
        return Request;
    }

    TSoftClassPtr<UGamePlatformUIScreen> WidgetClass = ResolveWidgetClass(*Definition);
    if (WidgetClass.IsNull())
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("MissingWidgetClass", "页面WidgetClass无效。"));
        return Request;
    }

    if (Definition->PreloadAssets.Num() > MaxPreloadAssetsPerScreen)
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("TooManyPreloads", "页面预加载资源数量超过平台上限。"));
        return Request;
    }

    TArray<FSoftObjectPath> AssetsToLoad;
    AssetsToLoad.Add(WidgetClass.ToSoftObjectPath());
    for (const TSoftObjectPtr<UObject>& Asset : Definition->PreloadAssets)
    {
        const FSoftObjectPath AssetPath = Asset.ToSoftObjectPath();
        if (AssetPath.IsValid())
        {
            AssetsToLoad.AddUnique(AssetPath);
        }
    }

    PendingRequests.Add(Request.RequestId, Request);
    if (IsValid(ViewModel))
    {
        PendingViewModels.Add(Request.RequestId, ViewModel);
    }

    TSharedPtr<FStreamableHandle> Handle =
        FGamePlatformAssetLoader::RequestAsyncLoad(
            AssetsToLoad,
            FStreamableDelegate::CreateUObject(
                this,
                &UGamePlatformUIManagerSubsystem::HandleScreenAssetsLoaded,
                Request.RequestId));

    if (!Handle.IsValid())
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("LoadRejected", "页面资源加载请求未被接受。"));
        CleanupPendingRequest(Request.RequestId, false);
        return Request;
    }

    PendingLoads.Add(Request.RequestId, Handle);
    return Request;
}

FGamePlatformUIAsyncRequest UGamePlatformUIManagerSubsystem::OpenRouteAsync(
    FName RouteId,
    UGamePlatformViewModelBase* ViewModel)
{
    TObjectPtr<UGamePlatformUIRouteDefinition>* RoutePtr = RouteDefinitions.Find(RouteId);
    UGamePlatformUIRouteDefinition* Route = RoutePtr ? RoutePtr->Get() : nullptr;

    if (!IsValid(Route))
    {
        FGamePlatformUIAsyncRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.ScreenId = NAME_None;
        Request.Generation = NextGeneration++;
        FailRequest(Request.RequestId, NAME_None, LOCTEXT("UnknownRoute", "UI路由未注册。"));
        return Request;
    }

    FText RouteError;
    if (!Route->ValidateDefinition(RouteError) ||
        !IsRouteTargetValid(*Route))
    {
        FGamePlatformUIAsyncRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.ScreenId = Route->ScreenId;
        Request.Generation = NextGeneration++;
        FailRequest(
            Request.RequestId,
            Route->ScreenId,
            RouteError.IsEmpty()
                ? LOCTEXT("InvalidRouteTarget", "UI路由目标页面无效。")
                : RouteError);
        return Request;
    }

    if (!ContextTags.HasAll(Route->RequiredTags) ||
        ContextTags.HasAny(Route->BlockedTags))
    {
        if (!Route->FallbackRouteId.IsNone() &&
            Route->FallbackRouteId != RouteId)
        {
            return OpenRouteAsync(Route->FallbackRouteId, ViewModel);
        }

        FGamePlatformUIAsyncRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.ScreenId = Route->ScreenId;
        Request.Generation = NextGeneration++;
        FailRequest(Request.RequestId, Route->ScreenId, LOCTEXT("RouteBlocked", "当前状态不允许使用该UI路由。"));
        return Request;
    }

    return OpenScreenAsync(Route->ScreenId, ViewModel);
}

bool UGamePlatformUIManagerSubsystem::CancelOpen(FGuid RequestId)
{
    if (!PendingRequests.Contains(RequestId))
    {
        return false;
    }

    CleanupPendingRequest(RequestId, true);
    return true;
}

bool UGamePlatformUIManagerSubsystem::CloseScreen(UGamePlatformUIScreen* Screen)
{
    if (!IsValid(Screen))
    {
        return false;
    }

    Screen->DeactivateWidget();
    return true;
}

bool UGamePlatformUIManagerSubsystem::AttachHUDWidget(UGamePlatformHUDWidget* Widget)
{
    return IsValid(RootLayout) && RootLayout->AddHUDWidget(Widget);
}

bool UGamePlatformUIManagerSubsystem::AttachToastWidget(UGamePlatformToastWidget* Widget)
{
    if (!IsValid(RootLayout) || !IsValid(Widget))
    {
        return false;
    }

    if (UWorld* World = GetWorld())
    {
        for (auto It = ActiveToasts.CreateIterator(); It; ++It)
        {
            if (!It.Value().IsValid())
            {
                if (FTimerHandle* Timer = ToastTimers.Find(It.Key()))
                {
                    World->GetTimerManager().ClearTimer(*Timer);
                }
                ToastTimers.Remove(It.Key());
                It.RemoveCurrent();
            }
        }
    }

    const bool bReplacingKnownKey =
        !Widget->ToastKey.IsNone() && ActiveToasts.Contains(Widget->ToastKey);
    if (!bReplacingKnownKey && ActiveToasts.Num() >= MaxActiveToasts)
    {
        return false;
    }

    FName EffectiveKey = Widget->ToastKey;
    if (EffectiveKey.IsNone())
    {
        EffectiveKey = FName(*FString::Printf(TEXT("Toast_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    }
    else if (const TWeakObjectPtr<UGamePlatformToastWidget>* Existing = ActiveToasts.Find(EffectiveKey))
    {
        if (UGamePlatformToastWidget* ExistingWidget = Existing->Get())
        {
            if (Widget->ToastPriority <= ExistingWidget->ToastPriority)
            {
                return false;
            }
            ExpireToast(EffectiveKey, *Existing);
        }
        else
        {
            ActiveToasts.Remove(EffectiveKey);
        }
    }

    if (!RootLayout->AddNotificationWidget(Widget))
    {
        return false;
    }

    const TWeakObjectPtr<UGamePlatformToastWidget> WeakWidget(Widget);
    ActiveToasts.Add(EffectiveKey, WeakWidget);

    if (Widget->DurationSeconds > KINDA_SMALL_NUMBER)
    {
        FTimerHandle TimerHandle;
        const TWeakObjectPtr<UGamePlatformUIManagerSubsystem> WeakThis(this);
        UWorld* World = GetWorld();
        if (!World)
        {
            return true;
        }
        World->GetTimerManager().SetTimer(
            TimerHandle,
            FTimerDelegate::CreateLambda([WeakThis, EffectiveKey, WeakWidget]()
            {
                if (UGamePlatformUIManagerSubsystem* Self = WeakThis.Get())
                {
                    Self->ExpireToast(EffectiveKey, WeakWidget);
                }
            }),
            Widget->DurationSeconds,
            false);
        ToastTimers.Add(EffectiveKey, TimerHandle);
    }

    return true;
}

void UGamePlatformUIManagerSubsystem::PrepareForTravel()
{
    TArray<FGuid> RequestIds;
    PendingRequests.GetKeys(RequestIds);
    for (const FGuid& RequestId : RequestIds)
    {
        CleanupPendingRequest(RequestId, true);
    }

    TArray<TWeakObjectPtr<UGamePlatformUIScreen>> Screens;
    ActiveScreenLeases.GetKeys(Screens);
    for (const TWeakObjectPtr<UGamePlatformUIScreen>& ScreenPtr : Screens)
    {
        if (TravelPersistentScreens.Contains(ScreenPtr))
        {
            continue;
        }

        if (UGamePlatformUIScreen* Screen = ScreenPtr.Get())
        {
            Screen->DeactivateWidget();
        }
    }

    TArray<FName> ToastKeys;
    ActiveToasts.GetKeys(ToastKeys);
    for (const FName& ToastKey : ToastKeys)
    {
        if (const TWeakObjectPtr<UGamePlatformToastWidget>* Toast = ActiveToasts.Find(ToastKey))
        {
            ExpireToast(ToastKey, *Toast);
        }
    }

    if (IsValid(RootLayout))
    {
        RootLayout->ClearHUD();
        RootLayout->ClearNotifications();
    }

    ++NextGeneration;
}

bool UGamePlatformUIManagerSubsystem::HasScreenDefinition(FName ScreenId) const
{
    return !ScreenId.IsNone() && ScreenDefinitions.Contains(ScreenId);
}

bool UGamePlatformUIManagerSubsystem::HasRouteDefinition(FName RouteId) const
{
    return !RouteId.IsNone() && RouteDefinitions.Contains(RouteId);
}

FName UGamePlatformUIManagerSubsystem::GetBackRouteId(FName RouteId) const
{
    const UGamePlatformUIRouteDefinition* Route =
        RouteDefinitions.FindRef(RouteId);
    return IsValid(Route) ? Route->BackRouteId : NAME_None;
}

bool UGamePlatformUIManagerSubsystem::IsDefinitionAllowed(
    const UGamePlatformUIScreenDefinition& Definition) const
{
    return ContextTags.HasAll(Definition.RequiredTags) &&
           !ContextTags.HasAny(Definition.BlockedTags);
}

bool UGamePlatformUIManagerSubsystem::IsRouteTargetValid(
    const UGamePlatformUIRouteDefinition& Definition) const
{
    const UGamePlatformUIScreenDefinition* Target =
        ScreenDefinitions.FindRef(Definition.ScreenId);
    return IsValid(Target) &&
        Target->Layer == Definition.Layer;
}

TSoftClassPtr<UGamePlatformUIScreen> UGamePlatformUIManagerSubsystem::ResolveWidgetClass(
    const UGamePlatformUIScreenDefinition& Definition) const
{
    const FName PlatformName(FPlatformProperties::IniPlatformName());
    if (const TSoftClassPtr<UGamePlatformUIScreen>* Variant =
        Definition.PlatformWidgetVariants.Find(PlatformName))
    {
        if (!Variant->IsNull())
        {
            return *Variant;
        }
    }

    return Definition.WidgetClass;
}

bool UGamePlatformUIManagerSubsystem::HasAnyRouteCycle() const
{
    TSet<FName> Visiting;
    TSet<FName> Visited;

    TFunction<bool(FName)> Visit = [&](FName RouteId) -> bool
    {
        if (RouteId.IsNone() || !RouteDefinitions.Contains(RouteId))
        {
            return false;
        }
        if (Visiting.Contains(RouteId))
        {
            return true;
        }
        if (Visited.Contains(RouteId))
        {
            return false;
        }

        Visiting.Add(RouteId);
        const UGamePlatformUIRouteDefinition* Route = RouteDefinitions.FindRef(RouteId);
        if (IsValid(Route))
        {
            if (Visit(Route->FallbackRouteId) || Visit(Route->BackRouteId))
            {
                return true;
            }
        }

        Visiting.Remove(RouteId);
        Visited.Add(RouteId);
        return false;
    };

    for (const TPair<FName, TObjectPtr<UGamePlatformUIRouteDefinition>>& Pair : RouteDefinitions)
    {
        if (Visit(Pair.Key))
        {
            return true;
        }
    }

    return false;
}

void UGamePlatformUIManagerSubsystem::HandlePreLoadMap(
    const FWorldContext& WorldContext,
    const FString& MapName)
{
    if (WorldContext.World() == GetWorld())
    {
        PrepareForTravel();
    }
}

void UGamePlatformUIManagerSubsystem::HandleScreenAssetsLoaded(FGuid RequestId)
{
    const FGamePlatformUIAsyncRequest* RequestPtr = PendingRequests.Find(RequestId);
    if (!RequestPtr)
    {
        return;
    }

    const FGamePlatformUIAsyncRequest Request = *RequestPtr;
    TObjectPtr<UGamePlatformUIScreenDefinition>* DefinitionPtr =
        ScreenDefinitions.Find(Request.ScreenId);
    UGamePlatformUIScreenDefinition* Definition =
        DefinitionPtr ? DefinitionPtr->Get() : nullptr;

    FText DefinitionError;
    if (!IsValid(Definition) ||
        !Definition->ValidateDefinition(DefinitionError) ||
        !IsValid(RootLayout) ||
        !IsDefinitionAllowed(*Definition))
    {
        FailRequest(RequestId, Request.ScreenId, LOCTEXT("StaleOpen", "页面打开条件已失效。"));
        CleanupPendingRequest(RequestId, false);
        return;
    }

    TSoftClassPtr<UGamePlatformUIScreen> SoftClass = ResolveWidgetClass(*Definition);
    UClass* LoadedClass = SoftClass.Get();
    UCommonActivatableWidgetStack* Stack =
        RootLayout->GetActivatableStack(Definition->Layer);

    if (!IsValid(LoadedClass) ||
        LoadedClass->HasAnyClassFlags(CLASS_Abstract) ||
        !LoadedClass->IsChildOf(UGamePlatformUIScreen::StaticClass()) ||
        !IsValid(Stack))
    {
        FailRequest(RequestId, Request.ScreenId, LOCTEXT("LoadFailed", "页面类或目标层加载失败。"));
        CleanupPendingRequest(RequestId, false);
        return;
    }

    UGamePlatformViewModelBase* ViewModel = nullptr;
    if (TObjectPtr<UGamePlatformViewModelBase>* ViewModelPtr =
        PendingViewModels.Find(RequestId))
    {
        ViewModel = ViewModelPtr->Get();
    }

    TSubclassOf<UGamePlatformUIScreen> ScreenClass = LoadedClass;
    UGamePlatformUIScreen* Screen =
        Stack->AddWidget<UGamePlatformUIScreen>(
            ScreenClass,
            [Definition, ViewModel](UGamePlatformUIScreen& NewScreen)
            {
                NewScreen.InitializeScreen(
                    Definition->ScreenId,
                    ViewModel,
                    Definition->DefaultFocusWidgetName,
                    Definition->InputMode,
                    Definition->PausePolicy);
            });

    if (!IsValid(Screen))
    {
        FailRequest(RequestId, Request.ScreenId, LOCTEXT("CreateFailed", "页面实例创建失败。"));
        CleanupPendingRequest(RequestId, false);
        return;
    }

    Screen->OnPlatformDeactivated().AddUObject(
        this,
        &UGamePlatformUIManagerSubsystem::HandleScreenDeactivated);

    if (TSharedPtr<FStreamableHandle>* LoadHandle = PendingLoads.Find(RequestId))
    {
        ActiveScreenLeases.Add(TWeakObjectPtr<UGamePlatformUIScreen>(Screen), *LoadHandle);
    }

    if (UGamePlatformUIInputPolicy::CanPauseWorld(GetWorld(), Definition->PausePolicy))
    {
        PauseScreens.Add(TWeakObjectPtr<UGamePlatformUIScreen>(Screen));
        RefreshStandalonePause();
    }

    if (Definition->bSurvivesTravel)
    {
        TravelPersistentScreens.Add(TWeakObjectPtr<UGamePlatformUIScreen>(Screen));
    }

    CleanupPendingRequest(RequestId, false);
    OnScreenOpened.Broadcast(RequestId, Request.ScreenId, Screen);
}

void UGamePlatformUIManagerSubsystem::HandleScreenDeactivated(
    UGamePlatformUIScreen* Screen)
{
    if (!IsValid(Screen))
    {
        return;
    }

    const FName ClosedScreenId = Screen->GetScreenId();
    const TWeakObjectPtr<UGamePlatformUIScreen> ScreenKey(Screen);
    ActiveScreenLeases.Remove(ScreenKey);
    PauseScreens.Remove(ScreenKey);
    TravelPersistentScreens.Remove(ScreenKey);
    RefreshStandalonePause();
    OnScreenClosed.Broadcast(ClosedScreenId);
}

void UGamePlatformUIManagerSubsystem::FailRequest(
    FGuid RequestId,
    FName ScreenId,
    const FText& Reason)
{
    OnScreenOpenFailed.Broadcast(RequestId, ScreenId, Reason);
}

void UGamePlatformUIManagerSubsystem::CleanupPendingRequest(
    FGuid RequestId,
    bool bCancelLoad)
{
    if (TSharedPtr<FStreamableHandle>* Handle = PendingLoads.Find(RequestId))
    {
        if (bCancelLoad)
        {
            FGamePlatformAssetLoader::Cancel(*Handle);
        }
    }

    PendingLoads.Remove(RequestId);
    PendingRequests.Remove(RequestId);
    PendingViewModels.Remove(RequestId);
}

void UGamePlatformUIManagerSubsystem::ExpireToast(
    FName ToastKey,
    TWeakObjectPtr<UGamePlatformToastWidget> ExpectedWidget)
{
    const TWeakObjectPtr<UGamePlatformToastWidget>* Current = ActiveToasts.Find(ToastKey);
    if (!Current || *Current != ExpectedWidget)
    {
        return;
    }

    if (UGamePlatformToastWidget* Widget = ExpectedWidget.Get())
    {
        Widget->RemoveFromParent();
    }

    if (UWorld* World = GetWorld())
    {
        if (FTimerHandle* Handle = ToastTimers.Find(ToastKey))
        {
            World->GetTimerManager().ClearTimer(*Handle);
        }
    }

    ToastTimers.Remove(ToastKey);
    ActiveToasts.Remove(ToastKey);
}

void UGamePlatformUIManagerSubsystem::RefreshStandalonePause()
{
    TArray<TWeakObjectPtr<UGamePlatformUIScreen>> StaleScreens;
    for (const TWeakObjectPtr<UGamePlatformUIScreen>& Screen : PauseScreens)
    {
        if (!Screen.IsValid())
        {
            StaleScreens.Add(Screen);
        }
    }
    for (const TWeakObjectPtr<UGamePlatformUIScreen>& Screen : StaleScreens)
    {
        PauseScreens.Remove(Screen);
    }

    APlayerController* PlayerController =
        GetLocalPlayer()->GetPlayerController(GetWorld());
    if (!IsValid(PlayerController))
    {
        return;
    }

    const bool bShouldPause = PauseScreens.Num() > 0;
    if (bShouldPause != bPauseAppliedByUI)
    {
        PlayerController->SetPause(bShouldPause);
        bPauseAppliedByUI = bShouldPause;
    }
}

#undef LOCTEXT_NAMESPACE
