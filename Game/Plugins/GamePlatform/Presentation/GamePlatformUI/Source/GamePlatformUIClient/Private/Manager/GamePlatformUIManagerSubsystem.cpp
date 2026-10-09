// 本文件属于GamePlatform平台层 GamePlatformUI，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Manager/GamePlatformUIScreenOpenCommitPolicy.h"

#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dialogs/GamePlatformToastWidget.h"
#include "HAL/PlatformProperties.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Routing/GamePlatformUIRouteDefinition.h"
#include "Screens/GamePlatformHUDWidget.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "Services/GamePlatformFeedbackService.h"
#include "Services/GamePlatformNotificationService.h"
#include "Services/GamePlatformWorldUIService.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "ViewModels/GamePlatformViewModelBase.h"

#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#define LOCTEXT_NAMESPACE "GamePlatformUI"

namespace
{
constexpr int32 MaxScreenDefinitions = 256;
constexpr int32 MaxRouteDefinitions = 512;
constexpr int32 MaxPendingOpenRequests = 32;
constexpr int32 MaxPreloadAssetsPerScreen = 32;
}

void UGamePlatformUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    LoadingScreenService = NewObject<UGamePlatformLoadingScreenService>(this);
    NotificationService = NewObject<UGamePlatformNotificationService>(this);
    FeedbackService = NewObject<UGamePlatformFeedbackService>(this);
    WorldUIService = NewObject<UGamePlatformWorldUIService>(this);

    if (IsValid(NotificationService))
    {
        NotificationService->Initialize(GetLocalPlayer());
    }
    if (IsValid(FeedbackService))
    {
        FeedbackService->Initialize(GetLocalPlayer());
    }
    if (IsValid(WorldUIService))
    {
        WorldUIService->Initialize(GetLocalPlayer());
    }

    ScreenDefinitions.Reserve(64);
    RouteDefinitions.Reserve(64);
    PendingRequests.Reserve(8);
    PendingViewModels.Reserve(8);
    PendingLoads.Reserve(8);
    PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
        this,
        &UGamePlatformUIManagerSubsystem::HandlePreLoadMap);
}

void UGamePlatformUIManagerSubsystem::Deinitialize()
{
    if (bClosing) return; // Closed/结果通知允许重入关闭，首次清理仍拥有全部账本。
    bClosing = true;
    ++RootLayoutGeneration;
    if (PreLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
        PreLoadMapHandle.Reset();
    }

    if (IsValid(NotificationService))
    {
        NotificationService->Clear();
        NotificationService->SetRootLayout(nullptr);
    }
    if (IsValid(FeedbackService))
    {
        FeedbackService->Clear();
        FeedbackService->SetRootLayout(nullptr);
    }
    if (IsValid(WorldUIService))
    {
        WorldUIService->Deinitialize();
    }

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

    LoadingScreenService = nullptr;
    NotificationService = nullptr;
    FeedbackService = nullptr;
    WorldUIService = nullptr;

    ClearScreenOwnership();

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
    if (bClosing || bReplacingRoot || !RootLayoutClass ||
        RootLayoutClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }

    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepService(this);
    APlayerController* PlayerController = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
    if (!IsValid(PlayerController))
    {
        return false;
    }

    TGuardValue<bool> ReplacingRoot(bReplacingRoot, true);
    const uint64 ReplacementGeneration = ++RootLayoutGeneration;
    const TStrongObjectPtr<UGamePlatformUILayerStack> PreviousRoot(RootLayout);
    const auto IsReplacementCurrent = [this, ReplacementGeneration, Previous = PreviousRoot.Get()]()
    {
        return !bClosing && RootLayoutGeneration == ReplacementGeneration && RootLayout == Previous;
    };
    const TStrongObjectPtr<UGamePlatformUILayerStack> NewRoot(
        CreateWidget<UGamePlatformUILayerStack>(PlayerController, RootLayoutClass));
    if (!NewRoot.IsValid() || !IsReplacementCurrent())
    {
        return false;
    }

    if (PreviousRoot.IsValid())
    {
        // 根布局替换前先清理所有依赖旧层容器的短生命周期服务实例。
        if (IsValid(NotificationService))
        {
            NotificationService->Clear();
            if (!IsReplacementCurrent()) return false;
        }
        if (IsValid(FeedbackService))
        {
            FeedbackService->Clear();
            if (!IsReplacementCurrent()) return false;
        }
        if (IsValid(WorldUIService))
        {
            WorldUIService->Clear();
            if (!IsReplacementCurrent()) return false;
        }
        ClearScreenOwnership();
        // OnScreenClosed是同步外部边界；监听Deinitialize后RootLayout可能已空，不能继续发布新布局。
        if (!IsReplacementCurrent()) return false;
        PreviousRoot->RemoveFromParent();
        if (!IsReplacementCurrent()) return false;
    }

    RootLayout = NewRoot.Get();
    if (!RootLayout->AddToPlayerScreen(0))
    {
        if (!bClosing && RootLayoutGeneration == ReplacementGeneration && RootLayout == NewRoot.Get()) RootLayout = nullptr;
        return false;
    }
    // AddToPlayerScreen可同步Construct/结果事件；关闭后的服务不会被SetRootLayout重新唤醒。
    if (bClosing || RootLayoutGeneration != ReplacementGeneration || RootLayout != NewRoot.Get()) return false;

    if (IsValid(NotificationService))
    {
        NotificationService->SetRootLayout(RootLayout);
        if (bClosing || RootLayoutGeneration != ReplacementGeneration || RootLayout != NewRoot.Get()) return false;
    }
    if (IsValid(FeedbackService))
    {
        FeedbackService->SetRootLayout(RootLayout);
        if (bClosing || RootLayoutGeneration != ReplacementGeneration || RootLayout != NewRoot.Get()) return false;
    }
    if (IsValid(WorldUIService))
    {
        WorldUIService->SetRootLayout(RootLayout);
        if (bClosing || RootLayoutGeneration != ReplacementGeneration || RootLayout != NewRoot.Get()) return false;
    }
    return true;
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

    if (bClosing || bReplacingRoot)
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("ScopeClosing", "页面作用域正在关闭或替换。"));
        return Request;
    }
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

    UGameInstance* GameInstance=GetLocalPlayer()->GetGameInstance();
    IGamePlatformDataService* Data=GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data)
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("MissingData", "统一资源服务不可用。"));
        CleanupPendingRequest(Request.RequestId, false);
        return Request;
    }
    FGamePlatformResult Accepted;
    const TWeakObjectPtr<UGamePlatformUIManagerSubsystem> WeakThis(this);
    const FGuid RequestId=Request.RequestId;
    const FGamePlatformDataLease Lease=Data->AcquireResources(AssetsToLoad,
        Definition->bSurvivesTravel ? EGamePlatformDataLifetime::Instance : EGamePlatformDataLifetime::World,
        this, [WeakThis,RequestId](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        {
            if (auto* Self=WeakThis.Get()) Self->HandleScreenAssetsLoaded(RequestId,CompletedLease,Result);
        }, Accepted);
    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        FailRequest(Request.RequestId, ScreenId, LOCTEXT("LoadRejected", "页面资源加载请求未被接受。"));
        CleanupPendingRequest(Request.RequestId, false);
        return Request;
    }
    PendingLoads.Add(Request.RequestId, Lease);
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

    const auto* StackPtr=ScreenStacks.Find(TWeakObjectPtr<UGamePlatformUIScreen>(Screen));
    auto* Stack=StackPtr ? StackPtr->Get() : nullptr;
    if (!Stack) return false;
    Stack->RemoveWidget(*Screen);
    ReconcileScreenMembership();
    return true;
}

bool UGamePlatformUIManagerSubsystem::AttachHUDWidget(UGamePlatformHUDWidget* Widget)
{
    return IsValid(RootLayout) && RootLayout->AddHUDWidget(Widget);
}

bool UGamePlatformUIManagerSubsystem::AttachToastWidget(
    UGamePlatformToastWidget* Widget)
{
    return IsValid(NotificationService) &&
        NotificationService->AttachToastWidget(Widget);
}

FGuid UGamePlatformUIManagerSubsystem::SubmitNotification(
    FGamePlatformUINotificationRequest Request,
    TSubclassOf<UGamePlatformNotificationWidget> WidgetClass)
{
    return IsValid(NotificationService)
        ? NotificationService->SubmitNotification(
            MoveTemp(Request),
            WidgetClass)
        : FGuid();
}

FGuid UGamePlatformUIManagerSubsystem::SubmitFeedback(
    FGamePlatformUIFeedbackRequest Request,
    TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass)
{
    return IsValid(FeedbackService)
        ? FeedbackService->SubmitFeedback(
            MoveTemp(Request),
            WidgetClass)
        : FGuid();
}

FGuid UGamePlatformUIManagerSubsystem::RegisterWorldUI(
    FGamePlatformWorldUIRequest Request,
    TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass)
{
    return IsValid(WorldUIService)
        ? WorldUIService->RegisterWorldUI(
            MoveTemp(Request),
            WidgetClass)
        : FGuid();
}

bool UGamePlatformUIManagerSubsystem::UpdateWorldUI(
    FGuid RequestId,
    FGamePlatformWorldUIRequest Request)
{
    return IsValid(WorldUIService) &&
        WorldUIService->UpdateWorldUI(
            RequestId,
            MoveTemp(Request));
}

bool UGamePlatformUIManagerSubsystem::UnregisterWorldUI(FGuid RequestId)
{
    return IsValid(WorldUIService) &&
        WorldUIService->UnregisterWorldUI(RequestId);
}

void UGamePlatformUIManagerSubsystem::PrepareForTravel()
{
    ++RootLayoutGeneration; // 在途构造即使仍看见原Root指针，也已属于旧世界切换代次。
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
            CloseScreen(Screen);
        }
    }

    if (IsValid(NotificationService))
    {
        NotificationService->Clear();
    }
    if (IsValid(FeedbackService))
    {
        FeedbackService->Clear();
    }
    if (IsValid(WorldUIService))
    {
        WorldUIService->Clear();
    }

    if (IsValid(RootLayout))
    {
        RootLayout->ClearHUD();
        RootLayout->ClearWorldProjection();
        RootLayout->ClearFeedback();
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
    const UGamePlatformUIScreenDefinition& Definition,
    bool bUseDefaultWidget) const
{
    // 同一请求最多由平台专属资源降级到默认资源一次，不允许失败时重复命中相同软路径。
    if (!bUseDefaultWidget)
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

bool UGamePlatformUIManagerSubsystem::BeginDefaultWidgetRetry(
    const FGamePlatformUIAsyncRequest& Request, UGamePlatformUIScreenDefinition& Definition,
    const FGamePlatformDataLease& PreviousLease)
{
    if (bClosing || bReplacingRoot || PendingDefaultWidgetRetries.Contains(Request.RequestId) ||
        Definition.WidgetClass.IsNull() || ResolveWidgetClass(Definition) == Definition.WidgetClass) return false;
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepManager(this);
    const TStrongObjectPtr<UGamePlatformUIScreenDefinition> KeepDefinition(&Definition);
    const TStrongObjectPtr<UGameInstance> GameInstance(GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr);
    IGamePlatformDataService* Data = GameInstance.IsValid() ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data) return false;
    const uint64 ExpectedLayoutGeneration = RootLayoutGeneration;
    const auto IsCurrent = [this, Request, ExpectedLayoutGeneration, Instance = GameInstance.Get(),
        ExpectedDefinition = &Definition]
    {
        const auto* Current = PendingRequests.Find(Request.RequestId);
        return !bClosing && !bReplacingRoot && RootLayoutGeneration == ExpectedLayoutGeneration &&
            Current && Current->Generation == Request.Generation && GetLocalPlayer() &&
            GetLocalPlayer()->GetGameInstance() == Instance &&
            ScreenDefinitions.FindRef(Request.ScreenId) == ExpectedDefinition;
    };
    if (!IsCurrent()) return false;
    PendingDefaultWidgetRetries.Add(Request.RequestId);
    // 先失效旧回包资格，再释放旧变体租约；Release中的通知不能让旧栈恢复已取消请求。
    const FGamePlatformDataLease ReleasedLease = PreviousLease;
    PendingLoads.Remove(Request.RequestId);
    Data->ReleaseResources(ReleasedLease);
    if (!IsCurrent()) return false;
    TArray<FSoftObjectPath> Assets;
    Assets.Add(Definition.WidgetClass.ToSoftObjectPath());
    for (const auto& Asset : Definition.PreloadAssets)
        if (Asset.ToSoftObjectPath().IsValid()) Assets.AddUnique(Asset.ToSoftObjectPath());
    FGamePlatformResult Accepted;
    const TWeakObjectPtr<UGamePlatformUIManagerSubsystem> WeakThis(this);
    const FGuid RequestId = Request.RequestId;
    // Data公开合同规定完成总是延后；受理返回后登记新完整租约，回包仍核LeaseId。
    const auto Lease = Data->AcquireResources(Assets,
        Definition.bSurvivesTravel ? EGamePlatformDataLifetime::Instance : EGamePlatformDataLifetime::World,
        this, [WeakThis, RequestId](const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
        { if (auto* Self = WeakThis.Get()) Self->HandleScreenAssetsLoaded(RequestId, Completed, Result); }, Accepted);
    if (!IsCurrent() || !Accepted.IsSuccess() || !Lease.IsValid())
    {
        if (Lease.IsValid()) Data->ReleaseResources(Lease);
        return false;
    }
    PendingLoads.Add(RequestId, Lease);
    return true;
}

void UGamePlatformUIManagerSubsystem::HandleScreenAssetsLoaded(FGuid RequestId,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    const FGamePlatformUIAsyncRequest* RequestPtr = PendingRequests.Find(RequestId);
    if (!RequestPtr) return;
    if (bClosing || bReplacingRoot)
    {
        CleanupPendingRequest(RequestId, true);
        return;
    }

    const FGamePlatformDataLease* ExpectedLease=PendingLoads.Find(RequestId);
    if (!ExpectedLease || ExpectedLease->LeaseId != Lease.LeaseId) return;
    if (ConstructingScreenRequests.Contains(RequestId)) return; // 同代完成重入不能启动第二构造。
    UGameInstance* GameInstance=GetLocalPlayer()->GetGameInstance();
    IGamePlatformDataService* Data=GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    const bool bLeaseReady = Result.IsSuccess() && Data &&
        Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded;
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

    const bool bDefaultRetry = PendingDefaultWidgetRetries.Contains(RequestId);
    TSoftClassPtr<UGamePlatformUIScreen> SoftClass =
        ResolveWidgetClass(*Definition, bDefaultRetry);
    UClass* LoadedClass = SoftClass.Get();
    UCommonActivatableWidgetStack* Stack =
        RootLayout->GetActivatableStack(Definition->Layer);
    const bool bInvalidWidgetClass =
        !IsValid(LoadedClass) ||
        LoadedClass->HasAnyClassFlags(CLASS_Abstract) ||
        !LoadedClass->IsChildOf(UGamePlatformUIScreen::StaticClass());

    if ((!bLeaseReady || bInvalidWidgetClass) &&
        !bDefaultRetry &&
        SoftClass != Definition->WidgetClass &&
        !Definition->WidgetClass.IsNull() &&
        IsValid(Stack))
    {
        if (BeginDefaultWidgetRetry(Request, *Definition, Lease)) return;
        if (bClosing || !PendingRequests.Contains(RequestId)) return;
    }

    if (!bLeaseReady || bInvalidWidgetClass || !IsValid(Stack))
    {
        // 默认资源也不可用时给出单次终态失败；不得无限重试或将空白页面当成成功。
        CleanupPendingRequest(RequestId, false);
        FailRequest(RequestId, Request.ScreenId, LOCTEXT("LoadFailed", "页面类、必需资源或目标层加载失败。"));
        return;
    }

    for (const FSoftObjectPath& Path : Lease.ResourcePaths)
    {
        if (!IsValid(Path.ResolveObject()))
        {
            CleanupPendingRequest(RequestId, false);
            FailRequest(RequestId, Request.ScreenId, LOCTEXT("PreloadMissing", "页面必需资源未加载完成。"));
            return;
        }
    }

    UGamePlatformViewModelBase* ViewModel = nullptr;
    if (TObjectPtr<UGamePlatformViewModelBase>* ViewModelPtr =
        PendingViewModels.Find(RequestId))
    {
        ViewModel = ViewModelPtr->Get();
    }

    if (!ObservedStacks.Contains(Stack))
    {
        Stack->OnDisplayedWidgetChanged().AddUObject(this, &UGamePlatformUIManagerSubsystem::HandleStackChanged);
        ObservedStacks.Add(Stack);
    }
    TSubclassOf<UGamePlatformUIScreen> ScreenClass = LoadedClass;
    FScreenOpenConstruction Construction;
    Construction.Request = Request;
    Construction.Lease = Lease;
    Construction.Root = RootLayout;
    Construction.Stack = Stack;
    Construction.Definition = Definition;
    Construction.World = GetWorld();
    Construction.GameInstance = GameInstance;
    Construction.LayoutGeneration = RootLayoutGeneration;
    // AddWidget的Init/失活/激活/切栈事件可同步取消、替换布局或GC；原对象必须撑到撤回结束。
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepManager(this);
    const TStrongObjectPtr<UGamePlatformUILayerStack> KeepRoot(RootLayout);
    const TStrongObjectPtr<UCommonActivatableWidgetStack> KeepStack(Stack);
    const TStrongObjectPtr<UGamePlatformUIScreenDefinition> KeepDefinition(Definition);
    const TStrongObjectPtr<UGamePlatformViewModelBase> KeepViewModel(ViewModel);
    ConstructingScreenRequests.Add(RequestId);
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

    const TStrongObjectPtr<UGamePlatformUIScreen> KeepScreen(Screen); // 撤回/暂停事件内GC也不能释放当前检查对象。
    CompleteScreenOpen(Construction, Screen);
}

bool UGamePlatformUIManagerSubsystem::CompleteScreenOpen(
    const FScreenOpenConstruction& Construction, UGamePlatformUIScreen* Screen)
{
    const FGuid RequestId = Construction.Request.RequestId;
    const auto* CurrentRequest = PendingRequests.Find(RequestId);
    const auto* CurrentLease = PendingLoads.Find(RequestId);
    auto* Stack = Construction.Stack.Get();
    auto* Definition = Construction.Definition.Get();
    auto* World = Construction.World.Get();
    auto* GameInstance = Construction.GameInstance.Get();
    auto* Player = GetLocalPlayer();
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    FGamePlatformUIScreenOpenCommitState State;
    State.bScreenValid = IsValid(Screen);
    State.bScopeCurrent = !bClosing && !bReplacingRoot && IsValid(World) &&
        !World->bIsTearingDown && World == GetWorld() && GameInstance && Player &&
        GameInstance == Player->GetGameInstance();
    State.bRequestCurrent = CurrentRequest && CurrentLease &&
        CurrentRequest->Generation == Construction.Request.Generation &&
        CurrentRequest->ScreenId == Construction.Request.ScreenId &&
        CurrentLease->LeaseId == Construction.Lease.LeaseId &&
        CurrentLease->Generation == Construction.Lease.Generation &&
        CurrentLease->ScopeId == Construction.Lease.ScopeId &&
        CurrentLease->IssuerProof == Construction.Lease.IssuerProof;
    State.bLayoutCurrent = Construction.LayoutGeneration == RootLayoutGeneration &&
        Construction.Root.IsValid() && Construction.Root.Get() == RootLayout &&
        IsValid(Definition) && ScreenDefinitions.FindRef(Construction.Request.ScreenId) == Definition &&
        IsDefinitionAllowed(*Definition) && IsValid(Stack) &&
        RootLayout->GetActivatableStack(Definition->Layer) == Stack;
    State.bLeaseSucceeded = Data &&
        Data->GetLeaseState(Construction.Lease) == EGamePlatformDataRequestState::Succeeded;
    State.bScreenInStack = IsValid(Stack) && State.bScreenValid && Stack->GetWidgetList().Contains(Screen);
    return State.Complete([&]
    {
        // 校验后先完整转移唯一租约并结束Pending，再调用可能发布用户事件的暂停/Opened逻辑。
        Screen->OnPlatformDeactivated().AddUObject(this, &UGamePlatformUIManagerSubsystem::HandleScreenDeactivated);
        Screen->OnActivated().AddUObject(this, &UGamePlatformUIManagerSubsystem::HandleScreenActivated, Screen);
        Screen->OnPlatformReleased().AddUObject(this, &UGamePlatformUIManagerSubsystem::HandleScreenReleased);
        // CommonUI非显示页移除也释放Slate；这一真实事件补足不触发DisplayedWidgetChanged的路径。
        Screen->OnSlateReleased().AddUObject(this, &UGamePlatformUIManagerSubsystem::HandleScreenReleased, Screen);
        ScreenStacks.Add(Screen, Stack);
        ActiveScreenLeases.Add(Screen, Construction.Lease);
        PendingLoads.Remove(RequestId);
        ConstructingScreenRequests.Remove(RequestId);
        if (Definition->bSurvivesTravel) TravelPersistentScreens.Add(Screen);
        CleanupPendingRequest(RequestId, false);
        if (Screen->IsActivated()) HandleScreenActivated(Screen);
        // SetPause/页面事件若进一步关闭作用域，不能对已经移除的实例发布Opened。
        if (!bClosing && !bReplacingRoot && IsValid(Screen) &&
            Construction.LayoutGeneration == RootLayoutGeneration && Construction.Root.Get() == RootLayout &&
            Construction.World.IsValid() && Construction.World.Get() == GetWorld() && !Construction.World->bIsTearingDown &&
            ActiveScreenLeases.Contains(Screen) && Stack->GetWidgetList().Contains(Screen))
            OnScreenOpened.Broadcast(RequestId, Construction.Request.ScreenId, Screen);
    }, [&]
    {
        // 此时还未登记Active，原容器负责撤回本次新控件；RemoveWidget也可同步重入。
        if (State.bScreenInStack)
        {
            // 失败构造没有入场资格，不保留正常退场动画；让CommonUI同步完成撤回再释放租约。
            const float PreviousDuration = Stack->GetTransitionDuration();
            Stack->SetTransitionDuration(0.0f);
            Stack->RemoveWidget(*Screen);
            if (Stack->GetTransitionDuration() == 0.0f) Stack->SetTransitionDuration(PreviousDuration);
        }
        ConstructingScreenRequests.Remove(RequestId);
        const auto* RemainingRequest = PendingRequests.Find(RequestId);
        const bool bSameRequest = RemainingRequest &&
            RemainingRequest->Generation == Construction.Request.Generation;
        const auto* RemainingLease = PendingLoads.Find(RequestId);
        if ((!RemainingRequest || bSameRequest) && (!RemainingLease ||
            RemainingLease->LeaseId == Construction.Lease.LeaseId))
            CleanupPendingRequest(RequestId, false);
        if (bSameRequest && !bClosing)
            FailRequest(RequestId, Construction.Request.ScreenId,
                LOCTEXT("OpenReentered", "页面构造期间请求、资源或布局已经失效。"));
    });
}

void UGamePlatformUIManagerSubsystem::HandleScreenDeactivated(
    UGamePlatformUIScreen* Screen)
{
    if (!IsValid(Screen))
    {
        return;
    }

    // 推入B只会暂时失活A；此事件仅调整显示暂停，不释放实例/Travel/Data账本。
    PauseScreens.Remove(Screen);
    RefreshStandalonePause();
}

void UGamePlatformUIManagerSubsystem::HandleScreenActivated(UGamePlatformUIScreen* Screen)
{
    if (!IsValid(Screen) || !ScreenStacks.Contains(Screen)) return;
    if (UGamePlatformUIInputPolicy::CanPauseWorld(GetWorld(), Screen->GetPausePolicy())) PauseScreens.Add(Screen);
    RefreshStandalonePause();
}
void UGamePlatformUIManagerSubsystem::HandleScreenReleased(UGamePlatformUIScreen* Screen)
{
    (void)Screen;
    // CommonUI池释放Slate可能发生在WidgetList移除之前，下一调度轮再核对真实成员关系。
    if (UWorld* World=GetWorld()) World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(this, &UGamePlatformUIManagerSubsystem::ReconcileScreenMembership));
}
void UGamePlatformUIManagerSubsystem::HandleStackChanged(UCommonActivatableWidget* DisplayedWidget)
{
    (void)DisplayedWidget;
    ReconcileScreenMembership();
}
void UGamePlatformUIManagerSubsystem::ReconcileScreenMembership()
{
    TArray<TWeakObjectPtr<UGamePlatformUIScreen>> Removed;
    for (const auto& Pair:ScreenStacks)
    {
        auto* Screen=Pair.Key.Get(); auto* Stack=Pair.Value.Get();
        if (!Screen || !Stack || !Stack->GetWidgetList().Contains(Screen)) Removed.Add(Pair.Key);
    }
    for (const auto& Key:Removed) RemoveScreenOwnership(Key);
}
bool UGamePlatformUIManagerSubsystem::IsScreenOwnedByStack(
    UGamePlatformUIScreen* Screen, UCommonActivatableWidgetStack* ExpectedStack) const
{
    if (bClosing || bReplacingRoot || !IsValid(Screen) || !IsValid(ExpectedStack)) return false;
    const auto* RegisteredStack = ScreenStacks.Find(Screen);
    // WidgetList保留旧页面不能替代实例账本；RemoveScreenOwnership在Closed前已撤精确Key。
    return RegisteredStack && RegisteredStack->Get() == ExpectedStack &&
        ExpectedStack->GetWidgetList().Contains(Screen);
}

void UGamePlatformUIManagerSubsystem::RemoveScreenOwnership(TWeakObjectPtr<UGamePlatformUIScreen> Key)
{
    if (!ScreenStacks.Remove(Key)) return; // 容器变化/Slate释放/退出交错只完成一次。
    FName ScreenId=NAME_None;
    if (auto* Screen=Key.Get())
    {
        ScreenId=Screen->GetScreenId();
        Screen->OnPlatformDeactivated().RemoveAll(this);
        Screen->OnActivated().RemoveAll(this);
        Screen->OnPlatformReleased().RemoveAll(this);
        Screen->OnSlateReleased().RemoveAll(this);
    }
    FGamePlatformDataLease Lease;
    if (ActiveScreenLeases.RemoveAndCopyValue(Key, Lease)) ReleaseScreenLease(Lease);
    PauseScreens.Remove(Key); TravelPersistentScreens.Remove(Key);
    RefreshStandalonePause();
    if (!ScreenId.IsNone()) OnScreenClosed.Broadcast(ScreenId);
}
void UGamePlatformUIManagerSubsystem::ClearScreenOwnership()
{
    TArray<TWeakObjectPtr<UGamePlatformUIScreen>> Screens; ScreenStacks.GetKeys(Screens);
    for (const auto& Key:Screens) RemoveScreenOwnership(Key);
    for (const auto& Stack:ObservedStacks)
        if (auto* ValidStack=Stack.Get()) ValidStack->OnDisplayedWidgetChanged().RemoveAll(this);
    ObservedStacks.Reset();
    ActiveScreenLeases.Reset(); PauseScreens.Reset(); TravelPersistentScreens.Reset();
}
void UGamePlatformUIManagerSubsystem::ReleaseScreenLease(const FGamePlatformDataLease& Lease)
{
    if (!Lease.IsValid()) return;
    auto* Player=GetLocalPlayer(); auto* GameInstance=Player ? Player->GetGameInstance() : nullptr;
    if (auto* Data=GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr) Data->ReleaseResources(Lease);
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
    (void)bCancelLoad; // Data Release同时覆盖Loading取消与成功释放，不直接操作StreamableManager。
    PendingDefaultWidgetRetries.Remove(RequestId);
    PendingRequests.Remove(RequestId);
    PendingViewModels.Remove(RequestId);
    if (ConstructingScreenRequests.Contains(RequestId)) return; // 同步构造尚未退栈，暂留租约防止提前卸载。
    FGamePlatformDataLease Lease;
    if (PendingLoads.RemoveAndCopyValue(RequestId, Lease)) ReleaseScreenLease(Lease);
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

    APlayerController* PlayerController = GetLocalPlayer()
        ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
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
