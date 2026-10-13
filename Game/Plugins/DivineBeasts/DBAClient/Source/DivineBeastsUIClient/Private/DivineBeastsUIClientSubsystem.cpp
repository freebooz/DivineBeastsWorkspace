// 项目层本地玩家UI组合：消费应用只读状态，注册平台页面/主题并桥接真实控制器生命周期。
// 不发送业务HTTP或持有玩法权威；退出撤销页面、适配器和委托，服务器目标不装配本模块。
#include "DivineBeastsUIClientSubsystem.h"
#include "Development/DivineBeastsDevelopmentLaunchMode.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Characters/DivineBeastsCharacterAppearanceComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProperties.h"
#include "Misc/CoreDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "CommonInputModeTypes.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "ViewModels/System/DivineBeastsSystemSettingsViewModel.h"

#include "Adapters/Application/DivineBeastsApplicationUIAdapter.h"
#include "Characters/DivineBeastsCharacterPreviewSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Dialogs/GamePlatformToastWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Layers/DivineBeastsRootLayout.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "Screens/GamePlatformHUDWidget.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "ViewModels/Boot/DivineBeastsBootViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"

namespace
{
    /** 第三层公共UI内容包的稳定RootLayout类路径；服务器目标不会启用该内容插件。 */
    const TSoftClassPtr<UGamePlatformUILayerStack> DefaultRootLayoutClass(
        FSoftObjectPath(
            TEXT("/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C")));
}

void UDivineBeastsUIClientSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    // 演示UI拥有独立根界面，不恢复正式登录RootLayout及业务页面。
    if (FDivineBeastsPluginDemoMode::IsEnabled())
    {
        Super::PlayerControllerChanged(NewPlayerController);
        return;
    }
    if (bUIShuttingDown) { Super::PlayerControllerChanged(NewPlayerController); return; }
    // 拒绝来自其他本地玩家的控制器，后续HUD/输入接线同样只消费本玩家作用域。
    if (NewPlayerController && NewPlayerController->GetLocalPlayer() != GetLocalPlayer()) NewPlayerController = nullptr;
    UpdateDefaultRootLayoutContext(GetLocalPlayer(), NewPlayerController);
    ObserveWorldInputRouter(GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>() : nullptr);
    CloseSystemMenu();
    if (auto* Previous = BoundMenuController.Get()) Previous->OnWorldEscapeRequested.Remove(WorldEscapeHandle);
    WorldEscapeHandle.Reset();
    BoundMenuController = Cast<ADivineBeastsWorldPlayerController>(NewPlayerController);
    if (auto* Current = BoundMenuController.Get())
        WorldEscapeHandle = Current->OnWorldEscapeRequested.AddUObject(this, &ThisClass::HandleWorldEscapeRequested);
    Super::PlayerControllerChanged(NewPlayerController);
    if (!IsValid(NewPlayerController))
    {
        // 世界退出先撤销上一控制器的数据来源；没有控制器时不创建根布局、不拉取流程快照。
        if (auto* Root = PlatformUI ? Cast<UDivineBeastsRootLayout>(PlatformUI->GetRootLayout()) : nullptr)
        { Root->RefreshForPlayerController(nullptr); }
        return;
    }
    // 世界退出可以取消在途页面；新控制器是真实可创建Widget的生命周期通知，消费现有快照即可恢复。
    EnsureConfiguredTheme();
    EnsureDefaultRootLayout();
    if (auto* Root = PlatformUI ? Cast<UDivineBeastsRootLayout>(PlatformUI->GetRootLayout()) : nullptr)
    {
        // Root跨Travel保留：真实控制器通知恢复作者HUD和Pawn订阅，不靠登录状态重复广播或Tick。
        Root->RefreshForPlayerController(NewPlayerController);
    }
    PullInitialState();
    SyncWorldInputBaseline();
}

void UDivineBeastsUIClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 在注册生产页面/主题和应用适配器前旁路；正式客户端默认行为不变。
    if (FDivineBeastsPluginDemoMode::IsEnabled())
    {
        return;
    }
    bUIShuttingDown = false;
    ResetDefaultRootLayoutLoad(false);
    RootLayoutPlayer.Reset();
    RootLayoutController.Reset();
    Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformUI = LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
        ObserveWorldInputRouter(LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>());
    }

    if (PlatformUI)
    {
        PlatformUI->OnScreenOpened.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened);
        PlatformUI->OnScreenOpenFailed.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed);
        PlatformUI->OnScreenClosed.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed);
        PlatformUI->OnThemeRequestFinished.AddUniqueDynamic(
            this, &UDivineBeastsUIClientSubsystem::HandleThemeRequestFinished);
    }

    ApplicationDeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &ThisClass::CloseSystemMenu);
    if (FSlateApplication::IsInitialized())
        SlateActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::HandleApplicationActivationChanged);
    RegisterDefaultScreenDefinitions();
    EnsureDefaultRootLayout();
    EnsureConfiguredTheme();

    // DBAClient 内部由项目 UI 层单向依赖项目 ApplicationFlow，
    // 不要求流程模块反向认识任何 Widget/ViewModel 类型。
    // 插件开发模式只使用本地UI基础设施，不创建会触发登录流程的应用Adapter。
    // 后续由Monolith制作的门户页面消费独立插件目录；不能伪造已认证ViewState。
    if (FDBADevelopmentLaunchMode::ResolveCurrent() != EDBADevelopmentLaunchMode::PluginDev)
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        ApplicationAdapter =
            NewObject<UDivineBeastsApplicationUIAdapter>(this);
        if (!ApplicationAdapter ||
            !ApplicationAdapter->Initialize(*LocalPlayer) ||
            !RegisterApplicationContract(
                TEXT("DivineBeasts.ApplicationFlow"),
                ApplicationAdapter,
                ApplicationAdapter).IsValid())
        {
            if (ApplicationAdapter)
            {
                ApplicationAdapter->Shutdown();
            }
            ApplicationAdapter = nullptr;
        }
    }
    // 子系统可能在本地控制器已存在后创建；仍通过同一事件接线路径完成一次绑定。
    if (!BoundMenuController.IsValid() && GetLocalPlayer()) PlayerControllerChanged(GetLocalPlayer()->GetPlayerController(GetWorld()));
}

void UDivineBeastsUIClientSubsystem::Deinitialize()
{
    // 退出先撤输入交接资格与精确Router委托，后续关闭回调不能发布新的世界基线。
    bUIShuttingDown = true;
    ResetDefaultRootLayoutLoad(false);
    RootLayoutPlayer.Reset();
    RootLayoutController.Reset();
    ObserveWorldInputRouter(nullptr);
    CloseSystemMenu();
    if (auto* Previous = BoundMenuController.Get()) Previous->OnWorldEscapeRequested.Remove(WorldEscapeHandle);
    BoundMenuController.Reset();
    WorldEscapeHandle.Reset();
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(ApplicationDeactivateHandle);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(SlateActivationHandle);
    ApplicationDeactivateHandle.Reset();
    SystemMenuVisibilityChanged.Clear();
    if (PlatformUI)
    {
        PlatformUI->OnThemeRequestFinished.RemoveDynamic(
            this, &UDivineBeastsUIClientSubsystem::HandleThemeRequestFinished);
        if (PendingThemeRequestId.IsValid()) PlatformUI->CancelThemeRequest(PendingThemeRequestId);
    }
    PendingThemeRequestId.Invalidate();
    if (PlatformUI && LoadingToken.IsValid())
    {
        if (UGamePlatformLoadingScreenService* LoadingService =
            PlatformUI->GetLoadingScreenService())
        {
            LoadingService->ReleaseToken(LoadingToken);
        }
        LoadingToken = {};
    }
    DetachContract();
    if (ApplicationAdapter)
    {
        ApplicationAdapter->Shutdown();
        ApplicationAdapter = nullptr;
    }
    if (PlatformUI)
    {
        PlatformUI->OnScreenOpened.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened);
        PlatformUI->OnScreenOpenFailed.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed);
        PlatformUI->OnScreenClosed.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed);

        if (OpeningPrimaryRequestId.IsValid())
        {
            PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        }
        if (UGamePlatformUIScreen* Active = ActivePrimaryScreen.Get())
        {
            PlatformUI->CloseScreen(Active);
        }
    }
    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
    ActivePrimaryScreen.Reset();
    ActivePrimaryScreenId = NAME_None;

    UnregisterDefaultScreenDefinitions();
    PlatformUI = nullptr;
    StateChanged.Clear();
    Super::Deinitialize();
}

void UDivineBeastsUIClientSubsystem::EnsureConfiguredTheme()
{
    if (bDefaultThemeRequested || !PlatformUI || !GConfig) return;
    FString ConfiguredId;
    if (!GConfig->GetString(TEXT("DivineBeasts.UI.Theme"), TEXT("DefaultThemeDefinitionId"), ConfiguredId, GGameIni) ||
        ConfiguredId.IsEmpty()) return;
    const FPrimaryAssetId ThemeId(ConfiguredId);
    if (!ThemeId.IsValid())
    {
        bDefaultThemeRequested = true;
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟主题编号格式不合法，保留原有界面。"));
        return;
    }
    FGamePlatformUIThemeContext Context;
    Context.PlatformId = FName(FPlatformProperties::PlatformName());
    PendingThemeRequestId = PlatformUI->RequestThemeAsync(ThemeId, Context);
    bDefaultThemeRequested = PendingThemeRequestId.IsValid();
}

void UDivineBeastsUIClientSubsystem::HandleThemeRequestFinished(
    FGuid RequestId, bool bSuccess, FText Reason)
{
    if (RequestId != PendingThemeRequestId) return;
    PendingThemeRequestId.Invalidate();
    if (!bSuccess)
    {
        // 仅诊断视觉资源失败，不跳转流程、不自动认证，也不逐帧重试。
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟主题切换未完成：%s"), *Reason.ToString());
    }
}

FDivineBeastsUIContractHandle
UDivineBeastsUIClientSubsystem::RegisterApplicationContract(
    FName AdapterId,
    UObject* InQuerySourceObject,
    UObject* InCommandPortObject)
{
    FDivineBeastsUIContractHandle Result;

    if (AdapterId.IsNone() ||
        !IsValid(InQuerySourceObject) ||
        !IsValid(InCommandPortObject) ||
        ContractHandle.IsValid() ||
        !InQuerySourceObject->GetClass()->ImplementsInterface(
            UDivineBeastsUIQuerySource::StaticClass()) ||
        !InCommandPortObject->GetClass()->ImplementsInterface(
            UDivineBeastsUICommandPort::StaticClass()))
    {
        return Result;
    }

    IDivineBeastsUIQuerySource* Query =
        Cast<IDivineBeastsUIQuerySource>(InQuerySourceObject);
    if (!Query)
    {
        return Result;
    }

    ContractAdapterId = AdapterId;
    QuerySourceObject = InQuerySourceObject;
    CommandPortObject = InCommandPortObject;
    ContractHandle.Id = FGuid::NewGuid();
    Result = ContractHandle;

    QueryStateHandle = Query->OnUIViewStateChanged().AddUObject(
        this,
        &UDivineBeastsUIClientSubsystem::HandleViewStateChanged);

    PullInitialState();
    return Result;
}

bool UDivineBeastsUIClientSubsystem::UnregisterApplicationContract(
    const FDivineBeastsUIContractHandle& Handle)
{
    if (!Handle.IsValid() ||
        !ContractHandle.IsValid() ||
        Handle.Id != ContractHandle.Id)
    {
        return false;
    }

    DetachContract();
    return true;
}

TArray<FName> UDivineBeastsUIClientSubsystem::GetRegisteredScreenIds() const
{
    return FDivineBeastsUIScreenCatalog::GetScreenIds();
}

FName UDivineBeastsUIClientSubsystem::GetRecommendedPrimaryScreenId() const
{
    return FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(ViewState, CharacterEntryScreenPreference);
}

bool UDivineBeastsUIClientSubsystem::RequestCharacterEntryScreen(FName ScreenId)
{
    if (!FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(ViewState, ScreenId)) { return false; }
    CharacterEntryScreenPreference = ScreenId;
    SyncPrimaryScreen();
    return true;
}

FGamePlatformUIAsyncRequest
UDivineBeastsUIClientSubsystem::OpenScreen(
    FName ScreenId,
    UDivineBeastsUIViewModel* ViewModel)
{
    return PlatformUI
        ? PlatformUI->OpenScreenAsync(ScreenId, ViewModel)
        : FGamePlatformUIAsyncRequest();
}

bool UDivineBeastsUIClientSubsystem::CancelScreenOpen(FGuid RequestId)
{
    return PlatformUI && PlatformUI->CancelOpen(RequestId);
}

bool UDivineBeastsUIClientSubsystem::CloseScreen(
    UGamePlatformUIScreen* Screen)
{
    return PlatformUI && PlatformUI->CloseScreen(Screen);
}

IGamePlatformDataService* UDivineBeastsUIClientSubsystem::ResolveRootLayoutDataService(UGameInstance* Instance) const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (RootDataServiceForTests) return RootDataServiceForTests;
#endif
    return IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UDivineBeastsUIClientSubsystem::ResetDefaultRootLayoutLoad(bool bKeepInstalledLease)
{
    check(IsInGameThread());
    ++RootLayoutLoadGeneration;
    // 根实例跨Travel保留时，其成功的Instance租约继续属于同一LocalPlayer。
    // Pending永不保留，控制器/玩家更换后的迟到完成不能安装旧根。
    if (bKeepInstalledLease && RootLayoutLoadState == ERootLayoutLoadState::Ready &&
        LeasedRootLayout.IsValid() && PlatformUI && PlatformUI->GetRootLayout() == LeasedRootLayout.Get())
        return;
    const FGamePlatformDataLease OldLease = DefaultRootLayoutLease;
    UGameInstance* Instance = RootLayoutLeaseInstance.Get();
    DefaultRootLayoutLease = {};
    RootLayoutLeaseInstance.Reset();
    RootLayoutRequestPath.Reset();
    LeasedRootLayout.Reset();
    RootLayoutLoadState = ERootLayoutLoadState::Idle;
    if (OldLease.IsValid())
        if (IGamePlatformDataService* Data = ResolveRootLayoutDataService(Instance))
            Data->ReleaseResources(OldLease);
}

void UDivineBeastsUIClientSubsystem::UpdateDefaultRootLayoutContext(
    ULocalPlayer* Player, APlayerController* Controller)
{
    // 控制器必须真正归当前玩家；不能用第零号玩家或其他分屏玩家替代空作用域。
    if (!IsValid(Controller) || Controller->GetLocalPlayer() != Player) Controller = nullptr;
    if (RootLayoutPlayer.Get() == Player && RootLayoutController.Get() == Controller) return;
    const bool bKeepInstalledLease = RootLayoutPlayer.Get() == Player &&
        (!Controller || RootLayoutLeaseInstance.Get() == Controller->GetGameInstance());
    ResetDefaultRootLayoutLoad(bKeepInstalledLease);
    RootLayoutPlayer = Player;
    RootLayoutController = Controller;
}

bool UDivineBeastsUIClientSubsystem::IsRootLayoutContextCurrent(uint64 ExpectedGeneration) const
{
    ULocalPlayer* Player = GetLocalPlayer();
    APlayerController* Controller = RootLayoutController.Get();
    UWorld* World = GetWorld();
    return !bUIShuttingDown && ExpectedGeneration == RootLayoutLoadGeneration &&
        IsValid(Player) && RootLayoutPlayer.Get() == Player && PlatformUI &&
        PlatformUI->GetLocalPlayer() == Player && IsValid(Controller) &&
        Controller->GetLocalPlayer() == Player && World && !World->bIsTearingDown &&
        Player->GetPlayerController(World) == Controller &&
        RootLayoutLeaseInstance.Get() == World->GetGameInstance();
}

bool UDivineBeastsUIClientSubsystem::InstallRootLayoutClass(
    TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass)
{
    check(IsInGameThread());
    ULocalPlayer* Player = GetLocalPlayer();
    APlayerController* Controller = Player ? Player->GetPlayerController(GetWorld()) : nullptr;
    if (bUIShuttingDown || bInstallingRootLayout || !PlatformUI ||
        PlatformUI->GetLocalPlayer() != Player || !IsValid(Controller) ||
        Controller->GetLocalPlayer() != Player || !RootLayoutClass ||
        RootLayoutClass->HasAnyClassFlags(CLASS_Abstract))
        return false;
    UpdateDefaultRootLayoutContext(Player, Controller);
    // 外部安装夺取当前请求资格；旧根仍显示时保留其成功租约，直到替换真正成功。
    ResetDefaultRootLayoutLoad(true);
    const uint64 Serial = RootLayoutLoadGeneration;
    bool bInstalled = false;
    {
        TGuardValue<bool> Installing(bInstallingRootLayout, true);
#if WITH_DEV_AUTOMATION_TESTS
        if (RootInstallerForTests) bInstalled = RootInstallerForTests(RootLayoutClass);
        else
#endif
        bInstalled = PlatformUI->InstallRootLayoutClass(RootLayoutClass);
    }
    if (bUIShuttingDown || Serial != RootLayoutLoadGeneration) return false;
    if (bInstalled)
    {
        ResetDefaultRootLayoutLoad(false);
        RootLayoutLoadState = ERootLayoutLoadState::Ready;
        // 仅在安装边界退出、Ready发布后恢复一次主路由；同步Construct不能递归申请或打开页面。
        SyncPrimaryScreen();
    }
    else if (!PlatformUI->GetRootLayout())
    {
        ResetDefaultRootLayoutLoad(false);
        RootLayoutLoadState = ERootLayoutLoadState::Failed;
    }
    return bInstalled;
}

bool UDivineBeastsUIClientSubsystem::EnsureDefaultRootLayout()
{
    check(IsInGameThread());
    ULocalPlayer* Player = GetLocalPlayer();
    if (bUIShuttingDown || !Player || !PlatformUI || PlatformUI->GetLocalPlayer() != Player)
        return false;
    if (bInstallingRootLayout) return PlatformUI->GetRootLayout() != nullptr;
    APlayerController* Controller = Player->GetPlayerController(GetWorld());
    UpdateDefaultRootLayoutContext(Player, Controller);
    if (PlatformUI->GetRootLayout())
    {
        if (DefaultRootLayoutLease.IsValid() && PlatformUI->GetRootLayout() != LeasedRootLayout.Get())
            ResetDefaultRootLayoutLoad(false); // 其他合法调用方已装根；撤销本调用者在途/旧根需求。
        RootLayoutLoadState = ERootLayoutLoadState::Ready;
        return true;
    }
    if (!RootLayoutController.IsValid()) return false;
    if (RootLayoutLoadState == ERootLayoutLoadState::Pending ||
        RootLayoutLoadState == ERootLayoutLoadState::Failed)
        return false; // 同一上下文合并在途请求；失败只等待控制器切换或显式外部安装。
    if (RootLayoutLoadState == ERootLayoutLoadState::Ready) ResetDefaultRootLayoutLoad(false);

    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = ResolveRootLayoutDataService(Instance);
    RootLayoutRequestPath = DefaultRootLayoutClass.ToSoftObjectPath();
#if WITH_DEV_AUTOMATION_TESTS
    if (RootLayoutPathForTests.IsValid()) RootLayoutRequestPath = RootLayoutPathForTests;
#endif
    if (!Data || !IsValid(Instance) || !World || World->bIsTearingDown || !RootLayoutRequestPath.IsValid())
    {
        RootLayoutLoadState = ERootLayoutLoadState::Failed;
        return false;
    }
    RootLayoutLeaseInstance = Instance;
    RootLayoutLoadState = ERootLayoutLoadState::Pending;
    const uint64 Serial = RootLayoutLoadGeneration;
    const TWeakObjectPtr<UDivineBeastsUIClientSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const FGamePlatformDataLease Lease = Data->AcquireResources(
        { RootLayoutRequestPath }, EGamePlatformDataLifetime::Instance, Player,
        [WeakThis, Serial](const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->HandleDefaultRootLayoutLoaded(Serial, Completed, Result);
        }, Accepted);
    if (!IsRootLayoutContextCurrent(Serial))
    {
        if (Lease.IsValid()) Data->ReleaseResources(Lease);
        return false;
    }
    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        if (Lease.IsValid()) Data->ReleaseResources(Lease);
        RootLayoutLoadState = ERootLayoutLoadState::Failed;
        return false;
    }
    DefaultRootLayoutLease = Lease;
    return false; // 受理是Pending；只能由延后完成发布Ready。
}

void UDivineBeastsUIClientSubsystem::HandleDefaultRootLayoutLoaded(uint64 ExpectedGeneration,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (ExpectedGeneration != RootLayoutLoadGeneration ||
        RootLayoutLoadState != ERootLayoutLoadState::Pending || !DefaultRootLayoutLease.IsValid() ||
        Lease.ScopeId != DefaultRootLayoutLease.ScopeId || Lease.LeaseId != DefaultRootLayoutLease.LeaseId ||
        Lease.Generation != DefaultRootLayoutLease.Generation || Lease.IssuerProof != DefaultRootLayoutLease.IssuerProof ||
        Lease.ResourcePaths != DefaultRootLayoutLease.ResourcePaths ||
        Lease.DefinitionId != DefaultRootLayoutLease.DefinitionId || Lease.Bundles != DefaultRootLayoutLease.Bundles)
        return;
    if (!IsRootLayoutContextCurrent(ExpectedGeneration))
    {
        ResetDefaultRootLayoutLoad(false);
        return;
    }
    if (PlatformUI->GetRootLayout())
    {
        EnsureDefaultRootLayout();
        SyncPrimaryScreen();
        return;
    }
    IGamePlatformDataService* Data = ResolveRootLayoutDataService(RootLayoutLeaseInstance.Get());
    UClass* LoadedClass = Result.IsSuccess() && Data &&
        Data->GetLeaseState(DefaultRootLayoutLease) == EGamePlatformDataRequestState::Succeeded
        ? Cast<UClass>(RootLayoutRequestPath.ResolveObject()) : nullptr;
    if (!LoadedClass || !LoadedClass->IsChildOf(UGamePlatformUILayerStack::StaticClass()) ||
        LoadedClass->HasAnyClassFlags(CLASS_Abstract))
    {
        ResetDefaultRootLayoutLoad(false);
        RootLayoutLoadState = ERootLayoutLoadState::Failed;
        return;
    }
    bool bInstalled = false;
    {
        TGuardValue<bool> Installing(bInstallingRootLayout, true);
#if WITH_DEV_AUTOMATION_TESTS
        if (RootInstallerForTests) bInstalled = RootInstallerForTests(LoadedClass);
        else
#endif
        bInstalled = PlatformUI->InstallRootLayoutClass(LoadedClass);
    }
    if (!IsRootLayoutContextCurrent(ExpectedGeneration)) return;
    if (!bInstalled || !PlatformUI->GetRootLayout())
    {
        ResetDefaultRootLayoutLoad(false);
        RootLayoutLoadState = ERootLayoutLoadState::Failed;
        return;
    }
    LeasedRootLayout = PlatformUI->GetRootLayout();
    RootLayoutLoadState = ERootLayoutLoadState::Ready;
    if (auto* Root = Cast<UDivineBeastsRootLayout>(LeasedRootLayout.Get()))
        Root->RefreshForPlayerController(RootLayoutController.Get());
    if (IsRootLayoutContextCurrent(ExpectedGeneration)) SyncPrimaryScreen();
}

UGamePlatformLoadingScreenService*
UDivineBeastsUIClientSubsystem::GetLoadingScreenService() const
{
    return PlatformUI ? PlatformUI->GetLoadingScreenService() : nullptr;
}

bool UDivineBeastsUIClientSubsystem::AttachHUDWidget(
    UGamePlatformHUDWidget* Widget)
{
    return PlatformUI && PlatformUI->AttachHUDWidget(Widget);
}

bool UDivineBeastsUIClientSubsystem::AttachToastWidget(
    UGamePlatformToastWidget* Widget)
{
    return PlatformUI && PlatformUI->AttachToastWidget(Widget);
}

UDivineBeastsUIViewModel*
UDivineBeastsUIClientSubsystem::CreateViewModel(FName ScreenId)
{
    UDivineBeastsUIViewModel* ViewModel = nullptr;

    // P0 页面使用类型明确的 ViewModel；其他现有页面继续复用兼容 ViewModel，
    // 保持增量迁移，不在本阶段一次性重写全部业务页面。
    if (ScreenId == TEXT("UI.Screen.Settings"))
    {
        ViewModel = NewObject<UDivineBeastsSystemSettingsViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.Login"))
    {
        ViewModel = NewObject<UDivineBeastsLoginViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.CharacterSelect"))
    {
        ViewModel = NewObject<UDivineBeastsCharacterSelectViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.CharacterCreate"))
    {
        ViewModel = NewObject<UDivineBeastsCharacterCreateViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.Boot"))
    {
        UDivineBeastsBootViewModel* BootViewModel =
            NewObject<UDivineBeastsBootViewModel>(this);
        if (BootViewModel)
        {
            // Boot 与世界切换都复用同一个平台 Loading Service。
            // 若当前阶段没有可量化进度，快照会保持 Progress < 0，
            // 蓝图只能显示不确定进度表现，禁止伪造百分比。
            BootViewModel->InitializeLoadingService(
                GetLoadingScreenService());
        }
        ViewModel = BootViewModel;
    }
    else if (ScreenId == TEXT("UI.Screen.LoadingTravel"))
    {
        UDivineBeastsLoadingViewModel* LoadingViewModel =
            NewObject<UDivineBeastsLoadingViewModel>(this);
        if (LoadingViewModel)
        {
            // Loading Service 属于当前 LocalPlayer 的平台 UI Manager，
            // ViewModel 只持有瞬态引用并在页面激活期间订阅事件。
            LoadingViewModel->InitializeLoadingService(
                GetLoadingScreenService());
        }
        ViewModel = LoadingViewModel;
    }
    else
    {
        ViewModel = NewObject<UDivineBeastsUIViewModel>(this);
    }

    if (ViewModel)
    {
        ViewModel->InitializeForScreen(this, ScreenId);
    }

    return ViewModel;
}

void UDivineBeastsUIClientSubsystem::SubmitCommand(
    FDivineBeastsUICommand Command,
    TFunction<void(const FDivineBeastsUICommandResult&)> Completion)
{
    FDivineBeastsUICommandResult Rejected;
    Rejected.RequestId = Command.RequestId;

    FString Error;
    if (!Command.IsValid(Error) || !CommandPortObject.IsValid())
    {
        Rejected.ErrorCode = TEXT("UI.Command.Invalid");
        if (Completion)
        {
            Completion(Rejected);
        }
        return;
    }

    IDivineBeastsUICommandPort* Port =
        Cast<IDivineBeastsUICommandPort>(CommandPortObject.Get());
    if (!Port)
    {
        Rejected.ErrorCode = TEXT("UI.Command.PortUnavailable");
        if (Completion)
        {
            Completion(Rejected);
        }
        return;
    }

    Port->SubmitUICommand(Command, MoveTemp(Completion));
}

bool UDivineBeastsUIClientSubsystem::CancelCommand(const FGuid& RequestId)
{
    if (!RequestId.IsValid() || !CommandPortObject.IsValid())
    {
        return false;
    }

    IDivineBeastsUICommandPort* Port =
        Cast<IDivineBeastsUICommandPort>(CommandPortObject.Get());
    return Port && Port->CancelUICommand(RequestId);
}

void UDivineBeastsUIClientSubsystem::RegisterDefaultScreenDefinitions()
{
    if (!PlatformUI)
    {
        return;
    }

    for (const FDivineBeastsUISurfaceDescriptor& Surface :
         FDivineBeastsUIScreenCatalog::GetSurfaces())
    {
        if (Surface.Kind != EDivineBeastsUISurfaceKind::Screen ||
            Surface.SurfaceId.IsNone() ||
            Surface.WidgetClassPath.IsEmpty())
        {
            continue;
        }

        UGamePlatformUIScreenDefinition* Definition =
            NewObject<UGamePlatformUIScreenDefinition>(this);
        Definition->ScreenId = Surface.SurfaceId;
        Definition->WidgetClass =
            TSoftClassPtr<UGamePlatformUIScreen>(
                FSoftObjectPath(Surface.WidgetClassPath));
        Definition->Layer = Surface.Layer;
        Definition->InputMode = Surface.InputMode;
        Definition->PausePolicy = Surface.PausePolicy;
        Definition->Transition = Surface.Transition;
        Definition->DefaultFocusWidgetName = Surface.DefaultFocusWidgetName;
        Definition->bSurvivesTravel = Surface.bSurvivesTravel;

        if (!Surface.MobileWidgetClassPath.IsEmpty())
        {
            const TSoftClassPtr<UGamePlatformUIScreen> MobileClass(
                FSoftObjectPath(Surface.MobileWidgetClassPath));

            // Android/iOS只改变布局和触控结构；共用同一ScreenId、ViewModel和业务状态。
            Definition->PlatformWidgetVariants.Add(
                TEXT("Android"),
                MobileClass);
            Definition->PlatformWidgetVariants.Add(
                TEXT("IOS"),
                MobileClass);
        }

        if (PlatformUI->RegisterScreenDefinition(Definition))
        {
            RegisteredDefinitions.Add(Definition);
        }
    }
}

void UDivineBeastsUIClientSubsystem::UnregisterDefaultScreenDefinitions()
{
    if (PlatformUI)
    {
        for (const UGamePlatformUIScreenDefinition* Definition :
             RegisteredDefinitions)
        {
            if (IsValid(Definition))
            {
                PlatformUI->UnregisterScreenDefinition(
                    Definition->ScreenId);
            }
        }
    }
    RegisteredDefinitions.Reset();
}

void UDivineBeastsUIClientSubsystem::HandleViewStateChanged(
    const FDivineBeastsUIViewState& NewState)
{
    const bool bNewRun =
        NewState.StateRunId.IsValid() &&
        NewState.StateRunId != ViewState.StateRunId;

    if (!bNewRun &&
        NewState.StateRunId == ViewState.StateRunId &&
        NewState.Revision < ViewState.Revision)
    {
        return;
    }

    // 页面偏好随账号运行和角色入口生命周期清理，旧账号/旧操作不得覆盖登录、传送或世界页面。
    if (bNewRun || !NewState.bAuthenticated ||
        (NewState.CurrentStep != TEXT("DBA.Flow.CharacterEntry") && NewState.CurrentStep != TEXT("DBA.Flow.CreateCharacter")))
    {
        CharacterEntryScreenPreference = NAME_None;
    }
    ViewState = NewState;
    if (!FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(ViewState)) CloseSystemMenu();
    StateChanged.Broadcast(ViewState);
    SyncLoadingService();
    EnsureDefaultRootLayout();
    SyncPrimaryScreen();
    SyncCharacterPreview();
    SyncWorldInputBaseline();
}

void UDivineBeastsUIClientSubsystem::PullInitialState()
{
    if (!QuerySourceObject.IsValid())
    {
        return;
    }

    if (IDivineBeastsUIQuerySource* Query =
        Cast<IDivineBeastsUIQuerySource>(QuerySourceObject.Get()))
    {
        HandleViewStateChanged(Query->GetUIViewState());
    }
}

void UDivineBeastsUIClientSubsystem::SyncLoadingService()
{
    if (!PlatformUI)
    {
        return;
    }

    UGamePlatformLoadingScreenService* LoadingService =
        PlatformUI->GetLoadingScreenService();
    if (!LoadingService)
    {
        return;
    }

    if (ViewState.Loading.bIsLoading)
    {
        if (!LoadingToken.IsValid())
        {
            LoadingToken = LoadingService->AcquireToken(
                ViewState.Loading.Stage,
                ViewState.Loading.Progress);
        }
        else
        {
            LoadingService->UpdateToken(
                LoadingToken,
                ViewState.Loading.Stage,
                ViewState.Loading.Progress);
        }
    }
    else if (LoadingToken.IsValid())
    {
        LoadingService->ReleaseToken(LoadingToken);
        LoadingToken = {};
    }
}

void UDivineBeastsUIClientSubsystem::SyncCharacterPreview()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UDivineBeastsCharacterPreviewSubsystem* Preview =
        LocalPlayer
            ? LocalPlayer->GetSubsystem<UDivineBeastsCharacterPreviewSubsystem>()
            : nullptr;
    if (!Preview)
    {
        return;
    }

    const bool bCharacterFrontEnd =
        ViewState.CurrentStep == TEXT("DBA.Flow.CharacterEntry") ||
        ViewState.CurrentStep == TEXT("DBA.Flow.CreateCharacter") ||
        ViewState.CurrentStep == TEXT("DBA.Flow.ValidateSelection");
    if (!bCharacterFrontEnd)
    {
        Preview->DeactivatePreviewScene();
        // 已确认选择仅用于本地Avatar表现；服务器准入/技能身份不由UI写入。
        if(ViewState.CurrentStep==TEXT("DBA.Flow.InWorld") && !ViewState.SelectedHeroDefinitionId.IsNone())
        {
            auto* Controller=GetLocalPlayer()->GetPlayerController(GetWorld());
            APawn* Pawn=Controller?Controller->GetPawn():nullptr;
            if(Pawn)
            {
                auto* Appearance=Pawn->FindComponentByClass<UDivineBeastsCharacterAppearanceComponent>();
                if(!Appearance){Appearance=NewObject<UDivineBeastsCharacterAppearanceComponent>(Pawn);Pawn->AddInstanceComponent(Appearance);Appearance->RegisterComponent();}
                Appearance->ApplyApprovedVisualHero(ViewState.SelectedHeroDefinitionId);
            }
        }
        return;
    }

    FName HeroDefinitionId = ViewState.SelectedHeroDefinitionId;
    if (HeroDefinitionId.IsNone())
    {
        for (const FDivineBeastsUICharacterItem& Character : ViewState.Characters)
        {
            if (Character.bSelected && !Character.HeroDefinitionId.IsNone())
            {
                HeroDefinitionId = Character.HeroDefinitionId;
                break;
            }
        }
    }
    if (HeroDefinitionId.IsNone() && !ViewState.Characters.IsEmpty())
    {
        HeroDefinitionId = ViewState.Characters[0].HeroDefinitionId;
    }
    if (HeroDefinitionId.IsNone() && !ViewState.CreateHeroOptions.IsEmpty())
    {
        HeroDefinitionId = ViewState.CreateHeroOptions[0].HeroDefinitionId;
    }

    if (HeroDefinitionId.IsNone())
    {
        Preview->ActivatePreviewScene();
    }
    else
    {
        Preview->PreviewHero(HeroDefinitionId);
    }
}

void UDivineBeastsUIClientSubsystem::SyncPrimaryScreen()
{
    if (bUIShuttingDown || bInstallingRootLayout || bSynchronizingPrimaryScreen ||
        !PlatformUI ||
        !PlatformUI->GetRootLayout())
    {
        return;
    }

    TGuardValue<bool> Guard(bSynchronizingPrimaryScreen, true);

    const FName DesiredScreenId =
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(ViewState, CharacterEntryScreenPreference);

    if (DesiredScreenId.IsNone())
    {
        if (OpeningPrimaryRequestId.IsValid())
        {
            PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        }
        OpeningPrimaryRequestId.Invalidate();
        OpeningPrimaryScreenId = NAME_None;

        if (UGamePlatformUIScreen* Active = ActivePrimaryScreen.Get())
        {
            ActivePrimaryScreen.Reset();
            ActivePrimaryScreenId = NAME_None;
            PlatformUI->CloseScreen(Active);
        }
        return;
    }

    if (ActivePrimaryScreen.IsValid() &&
        ActivePrimaryScreenId == DesiredScreenId)
    {
        return;
    }

    if (OpeningPrimaryScreenId == DesiredScreenId)
    {
        return;
    }

    if (OpeningPrimaryRequestId.IsValid())
    {
        PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        OpeningPrimaryRequestId.Invalidate();
        OpeningPrimaryScreenId = NAME_None;
    }

    if (!PlatformUI->HasScreenDefinition(DesiredScreenId))
    {
        return;
    }

    UDivineBeastsUIViewModel* ViewModel =
        CreateViewModel(DesiredScreenId);
    if (!ViewModel)
    {
        return;
    }

    // 先记录 ScreenId，再调用平台异步打开。平台可能在参数/Root/Layer无效时同步广播失败；
    // 失败处理会立即清空 OpeningPrimaryScreenId，因此调用返回后不能盲目记录一个已终结请求。
    OpeningPrimaryScreenId = DesiredScreenId;
    // 下一页成功加载前保留旧页。CommonUI激活新页时会先失活旧页，
    // 额外弱引用仅用于成功后从栈移除或失败时反馈，不延长资源租约生命周期。
    ReplacingPrimaryScreen = ActivePrimaryScreen;
    const FGamePlatformUIAsyncRequest Request =
        PlatformUI->OpenScreenAsync(
            DesiredScreenId,
            ViewModel);

    if (OpeningPrimaryScreenId == DesiredScreenId)
    {
        OpeningPrimaryRequestId = Request.RequestId;
    }
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened(
    FGuid RequestId,
    FName ScreenId,
    UGamePlatformUIScreen* Screen)
{
    if (ScreenId == OpeningSystemScreenId &&
        (!OpeningSystemRequestId.IsValid() || OpeningSystemRequestId == RequestId))
    {
        OpeningSystemScreenId = NAME_None;
        OpeningSystemRequestId.Invalidate();
        if (!FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(ViewState))
        { if (PlatformUI) PlatformUI->CloseScreen(Screen); return; }
        if (ScreenId == TEXT("UI.Screen.SystemMenu")) ActiveSystemMenu = Screen;
        else ActiveSystemSettings = Screen;
        SetSystemMenuVisible(true);
        return;
    }
    if (ScreenId != OpeningPrimaryScreenId ||
        (OpeningPrimaryRequestId.IsValid() &&
         RequestId != OpeningPrimaryRequestId))
    {
        return;
    }

    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
    ActivePrimaryScreen = Screen;
    ActivePrimaryScreenId = IsValid(Screen)
        ? ScreenId
        : NAME_None;
    if (UGamePlatformUIScreen* Previous = ReplacingPrimaryScreen.Get(); Previous && Previous != Screen)
    {
        PlatformUI->CloseScreen(Previous);
    }
    ReplacingPrimaryScreen.Reset();
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed(
    FGuid RequestId,
    FName ScreenId,
    FText Reason)
{
    if (ScreenId == OpeningSystemScreenId &&
        (!OpeningSystemRequestId.IsValid() || OpeningSystemRequestId == RequestId))
    {
        OpeningSystemScreenId = NAME_None;
        OpeningSystemRequestId.Invalidate();
        if (!ActiveSystemMenu.IsValid()) SetSystemMenuVisible(false);
        UE_LOG(LogTemp, Warning, TEXT("[DBA UI] System page open failed: %s"), *Reason.ToString());
        if (auto* Menu = Cast<UDivineBeastsUIScreen>(ActiveSystemMenu.Get()))
            Menu->ShowPageLoadError(FText::FromString(TEXT("设置页面暂时无法打开，请稍后重试。")));
        return;
    }
    // 内部资源错误只写诊断；用户文案不携带资源路径，也不污染业务快照。
    if (ScreenId != OpeningPrimaryScreenId ||
        (OpeningPrimaryRequestId.IsValid() &&
         RequestId != OpeningPrimaryRequestId))
    {
        return;
    }

    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
    UE_LOG(LogTemp, Warning, TEXT("[DBA UI] Page open failed: %s: %s"), *ScreenId.ToString(), *Reason.ToString());
    if (UDivineBeastsUIScreen* Previous = Cast<UDivineBeastsUIScreen>(ReplacingPrimaryScreen.Get()))
    {
        Previous->ShowPageLoadError(FText::FromString(TEXT("下一页面加载失败，请重新启动客户端后重试。")));
    }
    ReplacingPrimaryScreen.Reset();
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed(
    FName ScreenId)
{
    if (ScreenId == TEXT("UI.Screen.Settings")) ActiveSystemSettings.Reset();
    if (ScreenId == TEXT("UI.Screen.SystemMenu") && bSystemMenuVisible)
    { ActiveSystemMenu.Reset(); CloseSystemMenu(); }
    if (ScreenId == ActivePrimaryScreenId)
    {
        ActivePrimaryScreen.Reset();
        ActivePrimaryScreenId = NAME_None;
    }
    // 平台任何层的页面真正离栈都可能完成输入交接；实际活跃层/Router叶节点仍拥有优先权。
    SyncWorldInputBaseline();
}

void UDivineBeastsUIClientSubsystem::DetachContract()
{
    CloseSystemMenu();
    if (QuerySourceObject.IsValid() && QueryStateHandle.IsValid())
    {
        if (IDivineBeastsUIQuerySource* Query =
            Cast<IDivineBeastsUIQuerySource>(QuerySourceObject.Get()))
        {
            Query->OnUIViewStateChanged().Remove(QueryStateHandle);
        }
    }

    QueryStateHandle.Reset();
    QuerySourceObject.Reset();
    CommandPortObject.Reset();
    ContractAdapterId = NAME_None;
    ContractHandle = {};
    if (PlatformUI && LoadingToken.IsValid())
    {
        if (UGamePlatformLoadingScreenService* LoadingService =
            PlatformUI->GetLoadingScreenService())
        {
            LoadingService->ReleaseToken(LoadingToken);
        }
        LoadingToken = {};
    }
    ViewState = FDivineBeastsUIViewState();
    CharacterEntryScreenPreference = NAME_None;
}

// 世界菜单属于本地玩家UI层；CommonUI管理焦点/输入栈，项目世界控制器只撤销现有鼠标拖动。
void UDivineBeastsUIClientSubsystem::HandleWorldEscapeRequested() { HandleEscapeAction(); }
void UDivineBeastsUIClientSubsystem::HandleApplicationActivationChanged(bool bActive) { if (!bActive) CloseSystemMenu(); }

bool UDivineBeastsUIClientSubsystem::HandleEscapeAction()
{
    if (ActiveSystemSettings.IsValid()) return CloseSystemSettings();
    if (ActiveSystemMenu.IsValid() || !OpeningSystemScreenId.IsNone()) { CloseSystemMenu(); return true; }
    return FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(ViewState) && OpenSystemSurface(TEXT("UI.Screen.SystemMenu"));
}

bool UDivineBeastsUIClientSubsystem::OpenSystemSurface(FName ScreenId)
{
    if (!PlatformUI || !FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(ViewState) ||
        !OpeningSystemScreenId.IsNone() || !PlatformUI->HasScreenDefinition(ScreenId)) return false;
    OpeningSystemScreenId = ScreenId;
    const uint64 Generation = ++SystemMenuGeneration;
    // 必须先撤世界鼠标捕获，再入CommonUI栈；反过来会把新页UIOnly覆盖回世界输入模式。
    if (auto* Controller = BoundMenuController.Get()) Controller->SetWorldGameplayInputSuspended(true);
    const auto Request = PlatformUI->OpenScreenAsync(ScreenId, CreateViewModel(ScreenId));
    // 同步失败/同步完成会在调用返回前清掉身份，不能把已终结请求重新写回。
    if (SystemMenuGeneration == Generation && OpeningSystemScreenId == ScreenId) OpeningSystemRequestId = Request.RequestId;
    return Request.IsValid();
}

bool UDivineBeastsUIClientSubsystem::OpenSystemSettings()
{
    return ActiveSystemMenu.IsValid() && !ActiveSystemSettings.IsValid() && OpenSystemSurface(TEXT("UI.Screen.Settings"));
}

bool UDivineBeastsUIClientSubsystem::CloseSystemSettings()
{
    auto* Settings = ActiveSystemSettings.Get();
    if (!Settings || !PlatformUI) return false;
    ActiveSystemSettings.Reset();
    return PlatformUI->CloseScreen(Settings);
}

void UDivineBeastsUIClientSubsystem::SetSystemMenuVisible(bool bVisible)
{
    if (auto* Controller = BoundMenuController.Get()) Controller->SetWorldGameplayInputSuspended(bVisible);
    if (bSystemMenuVisible == bVisible) { if (!bVisible) SyncWorldInputBaseline(); return; }
    bSystemMenuVisible = bVisible;
    SystemMenuVisibilityChanged.Broadcast(bVisible);
    if (!bVisible) SyncWorldInputBaseline();
}

void UDivineBeastsUIClientSubsystem::CloseSystemMenu()
{
    const uint64 Generation = ++SystemMenuGeneration;
    const FGuid Request = OpeningSystemRequestId;
    OpeningSystemRequestId.Invalidate();
    OpeningSystemScreenId = NAME_None;
    auto* Settings = ActiveSystemSettings.Get();
    auto* Menu = ActiveSystemMenu.Get();
    ActiveSystemSettings.Reset();
    ActiveSystemMenu.Reset();
    if (PlatformUI)
    {
        if (Request.IsValid()) PlatformUI->CancelOpen(Request);
        if (Settings) PlatformUI->CloseScreen(Settings);
        if (Menu) PlatformUI->CloseScreen(Menu);
    }
    if (SystemMenuGeneration == Generation) SetSystemMenuVisible(false);
}

void UDivineBeastsUIClientSubsystem::ObserveWorldInputRouter(UCommonUIActionRouterBase* Router)
{
    // 只允许当前本地玩家的真实Router；跨玩家实例不能替本玩家发布输入配置。
    if (Router && Router->GetLocalPlayer() != GetLocalPlayer()) Router = nullptr;
    if (WorldInputRouter.Get() == Router) return;
    if (auto* PreviousRouter = WorldInputRouter.Get())
    {
        PreviousRouter->OnBoundActionsUpdated().Remove(WorldInputTreeHandle);
        PreviousRouter->OnActiveInputConfigChanged().Remove(WorldInputConfigHandle);
    }
    WorldInputTreeHandle.Reset();
    WorldInputConfigHandle.Reset();
    WorldInputRouter = Router;
    if (Router && !bUIShuttingDown)
    {
        WorldInputTreeHandle = Router->OnBoundActionsUpdated().AddUObject(this, &ThisClass::HandleWorldInputTreeChanged);
        WorldInputConfigHandle = Router->OnActiveInputConfigChanged().AddUObject(this, &ThisClass::HandleWorldInputConfigChanged);
    }
}

void UDivineBeastsUIClientSubsystem::HandleWorldInputTreeChanged() { SyncWorldInputBaseline(); }
void UDivineBeastsUIClientSubsystem::HandleWorldInputConfigChanged(FUIInputConfig Config)
{
    // CommonUI可在Slate延迟释放后继续持有旧Menu/默认Game配置；仅配置交接事件校正，不轮询。
    if (Config.GetInputMode() != ECommonInputMode::All || Config.GetMouseCaptureMode() != EMouseCaptureMode::NoCapture)
        SyncWorldInputBaseline();
}

void UDivineBeastsUIClientSubsystem::SyncWorldInputBaseline()
{
    auto* Router = WorldInputRouter.Get();
    if (bUIShuttingDown || bPublishingWorldInputBaseline || !Router ||
        !FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(ViewState) ||
        !OpeningPrimaryScreenId.IsNone() || OpeningPrimaryRequestId.IsValid() || !ActivePrimaryScreenId.IsNone() ||
        !OpeningSystemScreenId.IsNone() || OpeningSystemRequestId.IsValid() || bSystemMenuVisible ||
        ActiveSystemMenu.IsValid() || ActiveSystemSettings.IsValid()) return;
    if (auto* Primary = ActivePrimaryScreen.Get(); Primary && Primary->IsActivated()) return;
    if (auto* Previous = ReplacingPrimaryScreen.Get(); Previous && Previous->IsActivated()) return;
    if (auto* Root = PlatformUI ? PlatformUI->GetRootLayout() : nullptr)
    {
        for (const auto Layer : {EGamePlatformUILayer::Screen, EGamePlatformUILayer::Modal, EGamePlatformUILayer::System,
            EGamePlatformUILayer::Loading, EGamePlatformUILayer::Debug})
        {
            if (auto* Stack = Root->GetActivatableStack(Layer))
                for (auto* Widget : Stack->GetWidgetList()) if (IsValid(Widget) && Widget->IsActivated()) return;
        }
    }
    // 其他组件/竞技扩展的CommonUI叶节点同样拥有输入，不能因本项目主页面为空而强制接管。
    if (auto* Leaf = Router->GetLeafmostActivatableWidget(); Leaf && Leaf->IsActivated()) return;
    TGuardValue<bool> PublishingGuard(bPublishingWorldInputBaseline, true);
    const auto WorldConfig = UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameAndUI);
    // 经Router公共API更新真实ActiveInputConfig及原生视口，不仅调用PC.SetInputMode。
    // 引擎按新旧配置差量撤销Router自己的Ignore计数；本模块绝不Reset其他系统的锁。
    const bool bWasStaleConfig = Router->GetActiveInputMode() != ECommonInputMode::All || Router->GetActiveMouseCaptureMode() != EMouseCaptureMode::NoCapture;
    Router->SetActiveUIInputConfig(WorldConfig, this);
    if (bWasStaleConfig) UE_LOG(LogTemp, Log, TEXT("[DBA UI] World input handoff published GameAndUI/NoCapture through CommonUI Router."));
}
