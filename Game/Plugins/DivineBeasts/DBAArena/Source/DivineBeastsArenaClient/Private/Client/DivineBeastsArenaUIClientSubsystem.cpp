// 项目客户端每LocalPlayer竞技UI适配：借用平台页面/Data与MOBA事实，拥有World/Actor委托；退出清空投影，无Widget轮询。
#include "Client/DivineBeastsArenaUIClientSubsystem.h"

// GetRootLayout（获取界面根布局）与GetParent（获取面板父控件）需要完整UObject派生类型，
// 仅有前置声明时IsValid无法安全地执行指针转换；此处显式引用对应公共头文件。
#include "Layers/GamePlatformUILayerStack.h"
#include "Components/PanelWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Screens/GamePlatformUIScreen.h"
#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "UI/DivineBeastsArenaUIScreenCatalog.h"
#include "ViewModels/GamePlatformArenaViewModel.h"

void UDivineBeastsArenaUIClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bDeinitializing = false;
    ++SurfaceOperationGeneration;
    LastArenaSurfaceDemand = NAME_None;

    Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformUI =
            LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    }

    ArenaViewModel = NewObject<UGamePlatformArenaViewModel>(this);
    RegisteredDefinitions.Reserve(5);
    BoundPlayerStates.Reserve(10);

    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UDivineBeastsArenaUIClientSubsystem::HandleWorldCleanup);
    if (IsValid(PlatformUI))
    {
        PlatformUI->OnScreenOpened.AddDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpened);
        PlatformUI->OnScreenOpenFailed.AddDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpenFailed);
        PlatformUI->OnScreenClosed.AddDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenClosed);
    }

    RegisterArenaScreenDefinitions();
    RefreshArenaViewFromWorld();
}

void UDivineBeastsArenaUIClientSubsystem::Deinitialize()
{
    if (bDeinitializing) return;
    bDeinitializing = true;
    ++SurfaceOperationGeneration;
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    BindWorldReadinessEvents(nullptr);
    // 先撤销异步请求和HUD租约，再关闭页面并解除全局管理器事件。
    RemoveArenaHUD();
    CloseArenaScreen();
    if (IsValid(PlatformUI))
    {
        PlatformUI->OnScreenOpened.RemoveDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpened);
        PlatformUI->OnScreenOpenFailed.RemoveDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpenFailed);
        PlatformUI->OnScreenClosed.RemoveDynamic(
            this, &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenClosed);
    }
    UnbindArenaEvents();
    UnregisterArenaScreenDefinitions();

    ArenaViewModel = nullptr;
    PlatformUI = nullptr;
    Super::Deinitialize();
}

void UDivineBeastsArenaUIClientSubsystem::PlayerControllerChanged(
    APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    if (bDeinitializing) return;
    const uint64 Expected = ++SurfaceOperationGeneration;

    // ClientTravel / SeamlessTravel 后不再信任旧页面与世界HUD；后续只依据新世界事实装配。
    CloseArenaScreen();
    if (bDeinitializing || SurfaceOperationGeneration != Expected) return;
    RemoveArenaHUD();
    if (bDeinitializing || SurfaceOperationGeneration != Expected) return;
    FailedArenaSurfaceId = NAME_None;
    RefreshArenaViewFromWorld();
}

bool UDivineBeastsArenaUIClientSubsystem::RefreshArenaViewFromWorld()
{
    if (bDeinitializing) { return false; }
    UWorld* World = GetWorld();
    if (World && (World->bIsTearingDown || RetiredWorld.Get() == World)) { World = nullptr; }
    const uint64 BeforeBinding = SurfaceOperationGeneration;
    const bool bWorldChanged = BoundWorld.Get() != World;
    BindWorldReadinessEvents(World);
    if (bDeinitializing || BoundWorld.Get() != World ||
        SurfaceOperationGeneration != BeforeBinding + (bWorldChanged ? 1 : 0)) return false;
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> KeepService(this);
    const uint64 Expected = SurfaceOperationGeneration;
    AGamePlatformArenaGameState* GameState =
        World ? World->GetGameState<AGamePlatformArenaGameState>() : nullptr;
    if (!IsValid(GameState) || !IsValid(ArenaViewModel))
    {
        const bool bHadArenaWorld = BoundGameState.IsValid();
        UnbindArenaEvents();
        if (IsValid(ArenaViewModel) && (bHadArenaWorld || !ArenaViewModel->MatchId.IsEmpty()))
        { ArenaViewModel->ResetReplicatedArenaState(); }
        // 没有旧复制事实时保留前端匹配/传输流程；确实退出竞技才清旧投影并撤销表面。
        if (!bDeinitializing && SurfaceOperationGeneration == Expected) SyncArenaSurface();
        return false;
    }

    EnsureGameStateBinding(GameState);

    TArray<AGamePlatformArenaPlayerState*> PlayerStates;
    PlayerStates.Reserve(GameState->PlayerArray.Num());
    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        if (AGamePlatformArenaPlayerState* ArenaPlayerState =
            Cast<AGamePlatformArenaPlayerState>(BasePlayerState))
        {
            PlayerStates.Add(ArenaPlayerState);
        }
    }

    EnsurePlayerStateBindings(PlayerStates);
    ArenaViewModel->RefreshFromReplicatedState(
        GameState,
        PlayerStates);
    if (bDeinitializing || SurfaceOperationGeneration != Expected) return false;
    SyncArenaSurface();
    return true;
}

void UDivineBeastsArenaUIClientSubsystem::BindWorldReadinessEvents(UWorld* World)
{
    if (BoundWorld.Get() == World) { return; }
    const uint64 Expected = ++SurfaceOperationGeneration;
    if (BoundWorld.IsValid()) { BoundWorld->GameStateSetEvent.Remove(GameStateSetHandle); }
    // 跨世界先失效旧投影；保持新的匹配/连接流程由其拥有者后续事件更新。
    const bool bResetPreviousWorld = BoundWorld.IsValid();
    UnbindArenaEvents();
    BoundWorld = World; GameStateSetHandle.Reset();
    if (World)
    { GameStateSetHandle = World->GameStateSetEvent.AddWeakLambda(this, [this](AGameStateBase*) { RefreshArenaViewFromWorld(); }); }
    if (bResetPreviousWorld && IsValid(ArenaViewModel)) { ArenaViewModel->ResetReplicatedArenaState(); }
    if (bDeinitializing || SurfaceOperationGeneration != Expected) return;
    CloseArenaScreen();
    if (bDeinitializing || SurfaceOperationGeneration != Expected) return;
    RemoveArenaHUD();
}
void UDivineBeastsArenaUIClientSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
    if (BoundWorld.Get() != World) { return; }
    RetiredWorld = World;
    const uint64 BeforeBinding = SurfaceOperationGeneration;
    BindWorldReadinessEvents(nullptr);
    if (bDeinitializing || BoundWorld.IsValid() || SurfaceOperationGeneration != BeforeBinding + 1) return;
}

FName UDivineBeastsArenaUIClientSubsystem::ResolvePrimaryArenaSurfaceId() const
{
    if (bDeinitializing || !IsValid(ArenaViewModel))
    {
        return NAME_None;
    }

    switch (ArenaViewModel->FlowState)
    {
    case EGamePlatformArenaClientFlowState::Matchmaking:
        return TEXT("UI.Screen.Matchmaking");

    case EGamePlatformArenaClientFlowState::MatchFound:
    case EGamePlatformArenaClientFlowState::ReadyCheck:
        return TEXT("UI.Screen.MatchFoundReady");

    case EGamePlatformArenaClientFlowState::HeroSelection:
        return TEXT("UI.Screen.ArenaHeroSelection");

    case EGamePlatformArenaClientFlowState::InMatch:
        return TEXT("UI.HUD.Arena");

    case EGamePlatformArenaClientFlowState::Result:
        return TEXT("UI.Screen.PostMatchResult");

    case EGamePlatformArenaClientFlowState::Idle:
    case EGamePlatformArenaClientFlowState::Transferring:
    case EGamePlatformArenaClientFlowState::Connecting:
    case EGamePlatformArenaClientFlowState::Failed:
    default:
        // Transfer/Connect 使用公共 Loading UI；Failed 由公共错误/重连页面处理。
        return NAME_None;
    }
}

void UDivineBeastsArenaUIClientSubsystem::SetObservedArenaClientFlowState(
    EGamePlatformArenaClientFlowState InFlowState)
{
    if (!bDeinitializing && IsValid(ArenaViewModel))
    {
        ArenaViewModel->SetObservedClientFlowState(InFlowState);
        SyncArenaSurface();
    }
}


void UDivineBeastsArenaUIClientSubsystem::SyncArenaSurface()
{
    if (bDeinitializing || !IsValid(PlatformUI) || !IsValid(ArenaViewModel))
    {
        return;
    }

    const FName Desired = ResolvePrimaryArenaSurfaceId();
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> KeepService(this);
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepUI(PlatformUI);
    if (LastArenaSurfaceDemand != Desired)
    {
        LastArenaSurfaceDemand = Desired;
        ++SurfaceOperationGeneration;
    }
    const uint64 Expected = SurfaceOperationGeneration;
    const auto IsCurrent = [this, Expected, Desired, UI = KeepUI.Get()]
    {
        return !bDeinitializing && SurfaceOperationGeneration == Expected && PlatformUI == UI &&
            ResolvePrimaryArenaSurfaceId() == Desired;
    };
    if (FailedArenaSurfaceId != Desired)
    {
        // 状态切换后才允许重新检查曾缺失的资源，禁止复制事件导致无限尝试。
        FailedArenaSurfaceId = NAME_None;
    }
    if (Desired.IsNone())
    {
        CloseArenaScreen();
        if (!IsCurrent()) return;
        RemoveArenaHUD();
        return;
    }

    if (Desired == TEXT("UI.HUD.Arena"))
    {
        CloseArenaScreen();
        if (!IsCurrent()) return;
        EnsureArenaHUD();
        return;
    }

    RemoveArenaHUD();
    if (!IsCurrent()) return;
    if (!IsValid(PlatformUI->GetRootLayout()) ||
        FailedArenaSurfaceId == Desired)
    {
        return;
    }
    if (ActiveArenaSurfaceId == Desired && ActiveArenaScreen.IsValid())
    {
        return;
    }
    if (OpeningArenaSurfaceId == Desired &&
        (OpeningArenaRequestId.IsValid() || bDispatchingArenaScreenOpen))
    {
        return;
    }

    if (OpeningArenaRequestId.IsValid())
    {
        const FGuid OldRequestId = OpeningArenaRequestId;
        OpeningArenaRequestId.Invalidate();
        OpeningArenaSurfaceId = NAME_None;
        PlatformUI->CancelOpen(OldRequestId);
        if (!IsCurrent()) return;
    }

    const FDivineBeastsArenaUISurfaceDescriptor* Surface =
        FDivineBeastsArenaUIScreenCatalog::Find(Desired);
    if (!Surface || Surface->Kind != EDivineBeastsArenaUISurfaceKind::Screen ||
        !PlatformUI->HasScreenDefinition(Desired))
    {
        FailedArenaSurfaceId = Desired;
        return;
    }

    OpeningArenaSurfaceId = Desired;
    bArenaScreenOpenFailedDuringDispatch = false;
    bArenaScreenOpenedDuringDispatch = false;
    bDispatchingArenaScreenOpen = true;
    const FGamePlatformUIAsyncRequest Request =
        PlatformUI->OpenScreenAsync(Desired, ArenaViewModel);
    if (!IsCurrent())
    {
        // 同步拒绝通知可关闭/接管；返回受理不能把旧请求重新登记为在飞。
        if (Request.RequestId.IsValid()) KeepUI->CancelOpen(Request.RequestId);
        return;
    }
    bDispatchingArenaScreenOpen = false;

    // 平台可同步拒绝；Data加载完成按合同延后。派发期拒绝不能被受理返回栈重标为在飞。
    if (bArenaScreenOpenFailedDuringDispatch)
    {
        OpeningArenaSurfaceId = NAME_None;
        OpeningArenaRequestId.Invalidate();
        FailedArenaSurfaceId = Desired;
    }
    else if (bArenaScreenOpenedDuringDispatch)
    {
        OpeningArenaSurfaceId = NAME_None;
        OpeningArenaRequestId.Invalidate();
    }
    else if (Request.IsValid())
    {
        OpeningArenaRequestId = Request.RequestId;
    }
    else
    {
        OpeningArenaSurfaceId = NAME_None;
        FailedArenaSurfaceId = Desired;
    }
}

void UDivineBeastsArenaUIClientSubsystem::CloseArenaScreen()
{
    const FGuid RequestId = OpeningArenaRequestId;
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> UI(PlatformUI);
    const TStrongObjectPtr<UGamePlatformUIScreen> Screen(ActiveArenaScreen.Get());
    OpeningArenaRequestId.Invalidate();
    OpeningArenaSurfaceId = NAME_None;
    bDispatchingArenaScreenOpen = false;
    bArenaScreenOpenFailedDuringDispatch = false;
    bArenaScreenOpenedDuringDispatch = false;
    ActiveArenaScreen.Reset();
    ActiveArenaScreenStack.Reset();
    ActiveArenaSurfaceId = NAME_None;

    if (UI.IsValid() && RequestId.IsValid())
    {
        UI->CancelOpen(RequestId);
    }

    if (UI.IsValid() && Screen.IsValid())
    {
        // 只关闭本子系统打开的页面，不触碰DBAClient公共登录与世界页面。
        UI->CloseScreen(Screen.Get());
    }
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpened(
    FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen)
{
    const bool bExpectedRequest =
        (OpeningArenaRequestId.IsValid() && OpeningArenaRequestId == RequestId) ||
        (bDispatchingArenaScreenOpen && OpeningArenaSurfaceId == ScreenId);
    if (bDeinitializing || !bExpectedRequest || !IsValid(Screen))
    {
        return;
    }

    if (bDispatchingArenaScreenOpen)
    {
        bArenaScreenOpenedDuringDispatch = true;
    }
    OpeningArenaRequestId.Invalidate();
    OpeningArenaSurfaceId = NAME_None;

    // 确认异步回调仍对应最新阶段；状态已切换的页面必须马上撤销。
    if (ResolvePrimaryArenaSurfaceId() != ScreenId)
    {
        if (IsValid(PlatformUI))
        {
            PlatformUI->CloseScreen(Screen);
        }
        return;
    }

    const auto* Surface = FDivineBeastsArenaUIScreenCatalog::Find(ScreenId);
    auto* Root = IsValid(PlatformUI) ? PlatformUI->GetRootLayout() : nullptr;
    auto* Stack = IsValid(Root) && Surface ? Root->GetActivatableStack(Surface->Layer) : nullptr;
    // 更早的Opened观察者可换Root；只有具体实例仍在当前实际层栈，才登记为本域拥有。
    if (!IsValid(Stack) || !PlatformUI->IsScreenOwnedByStack(Screen, Stack))
    {
        if (IsValid(PlatformUI)) PlatformUI->CloseScreen(Screen);
        return;
    }
    const TStrongObjectPtr<UGamePlatformUIScreen> Previous(ActiveArenaScreen.Get());
    ActiveArenaScreen = Screen;
    ActiveArenaScreenStack = Stack;
    ActiveArenaSurfaceId = ScreenId;
    FailedArenaSurfaceId = NAME_None;
    if (Previous.IsValid() && Previous.Get() != Screen && IsValid(PlatformUI))
    {
        PlatformUI->CloseScreen(Previous.Get());
    }
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpenFailed(
    FGuid RequestId, FName ScreenId, FText Reason)
{
    const bool bExpectedRequest =
        (OpeningArenaRequestId.IsValid() && OpeningArenaRequestId == RequestId) ||
        (bDispatchingArenaScreenOpen && OpeningArenaSurfaceId == ScreenId);
    if (bDeinitializing || !bExpectedRequest)
    {
        return;
    }

    // 保留旧页面并记录缺失表面；同一阶段其他复制事件不会重复发起失败请求。
    if (bDispatchingArenaScreenOpen)
    {
        bArenaScreenOpenFailedDuringDispatch = true;
    }
    OpeningArenaRequestId.Invalidate();
    OpeningArenaSurfaceId = NAME_None;
    FailedArenaSurfaceId = ScreenId;
    UE_LOG(LogTemp, Warning,
        TEXT("DBAArena UI页面加载失败：%s；原因：%s"),
        *ScreenId.ToString(), *Reason.ToString());
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenClosed(FName ScreenId)
{
    if (bDeinitializing) return;
    if (ScreenId == ActiveArenaSurfaceId)
    {
        auto* Owned = ActiveArenaScreen.Get();
        auto* OriginalStack = ActiveArenaScreenStack.Get();
        // 全局通知只带内容ID；平台具体实例账本与登记原栈共同证明后继仍拥有。
        // Root撤账早于WidgetList清空，不能仅凭旧成员仍在容器忽略自有实例的真正关闭。
        // 暂失活不改变归属，避免IsActivated把被同栈新页覆盖的自有页误清。
        if (IsValid(PlatformUI) && PlatformUI->IsScreenOwnedByStack(Owned, OriginalStack)) return;
        ActiveArenaScreen.Reset();
        ActiveArenaScreenStack.Reset();
        ActiveArenaSurfaceId = NAME_None;
    }
}

void UDivineBeastsArenaUIClientSubsystem::EnsureArenaHUD()
{
    if (bDeinitializing || ResolvePrimaryArenaSurfaceId() != TEXT("UI.HUD.Arena") ||
        !IsValid(PlatformUI) || !IsValid(PlatformUI->GetRootLayout()) ||
        !IsValid(ArenaViewModel) || FailedArenaSurfaceId == TEXT("UI.HUD.Arena")) return;
    if (IsValid(ActiveArenaHUD) && IsValid(ActiveArenaHUD->GetParent())) return;
    if (PendingArenaHUDLoad.IsValid()) return;
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> KeepService(this);
    if (ActiveArenaHUDLease.IsValid() || IsValid(ActiveArenaHUD))
    {
        RemoveArenaHUD();
        if (bDeinitializing || PendingArenaHUDLoad.IsValid() ||
            ResolvePrimaryArenaSurfaceId() != TEXT("UI.HUD.Arena")) return;
    }
    const auto* Surface = FDivineBeastsArenaUIScreenCatalog::Find(TEXT("UI.HUD.Arena"));
    const FSoftObjectPath Path(Surface ? Surface->WidgetClassPath : FString());
    UWorld* World = GetWorld();
    UGameInstance* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Path.IsValid() || !World || World->bIsTearingDown || BoundWorld.Get() != World || !Data)
    {
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        return;
    }
    const uint32 RequestGeneration = ++ArenaHUDRequestGeneration;
    PendingArenaHUDInstance = Instance;
    PendingArenaHUDWorld = World;
    FGamePlatformResult Accepted;
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> WeakThis(this);
    const TStrongObjectPtr<UGameInstance> KeepInstance(Instance);
    // HUD软类归同一Data资源服务；完成合同为延后GT回调，受理后登记完整租约，不同步读盘。
    const auto Lease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World,
        this, [WeakThis, RequestGeneration](const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
        { if (auto* Self = WeakThis.Get()) Self->HandleArenaHUDLoaded(RequestGeneration, Completed, Result); }, Accepted);
    if (bDeinitializing || RequestGeneration != ArenaHUDRequestGeneration || BoundWorld.Get() != World ||
        ResolvePrimaryArenaSurfaceId() != TEXT("UI.HUD.Arena"))
    {
        if (Lease.IsValid()) Data->ReleaseResources(Lease);
        return;
    }
    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        PendingArenaHUDInstance.Reset(); PendingArenaHUDWorld.Reset();
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        if (Lease.IsValid()) Data->ReleaseResources(Lease);
        return;
    }
    PendingArenaHUDLoad = Lease;
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaHUDLoaded(uint32 RequestGeneration,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    if (RequestGeneration != ArenaHUDRequestGeneration || PendingArenaHUDLoad.LeaseId != Lease.LeaseId) return;
    const FGamePlatformDataLease OwnedLease = Lease;
    const TWeakObjectPtr<UGameInstance> Instance = PendingArenaHUDInstance;
    const TWeakObjectPtr<UWorld> World = PendingArenaHUDWorld;
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> KeepService(this);
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> UI(PlatformUI);
    const TStrongObjectPtr<UGamePlatformArenaViewModel> ViewModel(ArenaViewModel);
    const TStrongObjectPtr<UGamePlatformUILayerStack> Root(UI.IsValid() ? UI->GetRootLayout() : nullptr);
    const auto IsCurrent = [this, RequestGeneration, OwnedLease, Instance, World, UI = UI.Get(), Root = Root.Get()]
    {
        return !bDeinitializing && RequestGeneration == ArenaHUDRequestGeneration &&
            PendingArenaHUDLoad.LeaseId == OwnedLease.LeaseId && World.IsValid() && !World->bIsTearingDown &&
            BoundWorld == World && GetWorld() == World.Get() && Instance.IsValid() && GetLocalPlayer() &&
            GetLocalPlayer()->GetGameInstance() == Instance.Get() && PlatformUI == UI && IsValid(UI) &&
            IsValid(Root) && UI->GetRootLayout() == Root && ResolvePrimaryArenaSurfaceId() == TEXT("UI.HUD.Arena");
    };
    const auto Discard = [this, RequestGeneration, OwnedLease, Instance](UGamePlatformMobaArenaHUDBase* HUD, bool bFailure)
    {
        // 先摘本请求字段，再进入Widget/Data外部边界；旧完成只释放自己的租约，不清新请求。
        if (RequestGeneration == ArenaHUDRequestGeneration && PendingArenaHUDLoad.LeaseId == OwnedLease.LeaseId)
        {
            PendingArenaHUDLoad = {}; PendingArenaHUDInstance.Reset(); PendingArenaHUDWorld.Reset();
            if (bFailure && !bDeinitializing) FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        }
        if (IsValid(HUD)) HUD->RemoveFromParent();
        ReleaseArenaHUDLease(OwnedLease, Instance);
    };
    IGamePlatformDataService* Data = Instance.IsValid() ? IGamePlatformDataService::Get(*Instance.Get()) : nullptr;
    if (!IsCurrent() || !Result.IsSuccess() || !Data || Data->GetLeaseState(OwnedLease) != EGamePlatformDataRequestState::Succeeded)
    { Discard(nullptr, IsCurrent()); return; }
    const auto* Surface = FDivineBeastsArenaUIScreenCatalog::Find(TEXT("UI.HUD.Arena"));
    TSoftClassPtr<UGamePlatformMobaArenaHUDBase> Class(FSoftObjectPath(Surface ? Surface->WidgetClassPath : FString()));
    UClass* LoadedClass = Class.Get();
    APlayerController* Controller = GetLocalPlayer()->GetPlayerController(World.Get());
    if (!IsValid(LoadedClass) || LoadedClass->HasAnyClassFlags(CLASS_Abstract) ||
        !LoadedClass->IsChildOf(UGamePlatformMobaArenaHUDBase::StaticClass()) || !IsValid(Controller))
    {
        Discard(nullptr, true);
        UE_LOG(LogTemp, Warning, TEXT("DBAArena HUD资源不存在或父类不合法，等待真实Monolith资产交付。"));
        return;
    }
    const TStrongObjectPtr<UGamePlatformMobaArenaHUDBase> HUD(CreateWidget<UGamePlatformMobaArenaHUDBase>(Controller, LoadedClass));
    if (!HUD.IsValid() || !IsCurrent()) { Discard(HUD.Get(), IsCurrent()); return; }
    HUD->InitializeArenaViewModel(ViewModel.Get());
    if (!IsCurrent()) { Discard(HUD.Get(), false); return; }
    const bool bAttached = UI->AttachHUDWidget(HUD.Get());
    if (!bAttached || !IsCurrent()) { Discard(HUD.Get(), !bAttached && IsCurrent()); return; }
    // 同步Init/挂载均仍属同一World、布局与请求后，原子转移唯一Data租约到已显示HUD。
    ActiveArenaHUD = HUD.Get(); ActiveArenaHUDLease = OwnedLease; ActiveArenaHUDInstance = Instance;
    PendingArenaHUDLoad = {}; PendingArenaHUDInstance.Reset(); PendingArenaHUDWorld.Reset();
    FailedArenaSurfaceId = NAME_None;
}

void UDivineBeastsArenaUIClientSubsystem::ReleaseArenaHUDLease(
    const FGamePlatformDataLease& Lease, TWeakObjectPtr<UGameInstance> Instance)
{
    if (Lease.IsValid() && Instance.IsValid())
        if (auto* Data = IGamePlatformDataService::Get(*Instance.Get())) Data->ReleaseResources(Lease);
}

void UDivineBeastsArenaUIClientSubsystem::RemoveArenaHUD()
{
    ++ArenaHUDRequestGeneration;
    const auto Pending = PendingArenaHUDLoad; const auto PendingInstance = PendingArenaHUDInstance;
    const auto Active = ActiveArenaHUDLease; const auto ActiveInstance = ActiveArenaHUDInstance;
    const TStrongObjectPtr<UGamePlatformMobaArenaHUDBase> HUD(ActiveArenaHUD);
    PendingArenaHUDLoad = {}; ActiveArenaHUDLease = {}; ActiveArenaHUD = nullptr;
    PendingArenaHUDInstance.Reset(); ActiveArenaHUDInstance.Reset(); PendingArenaHUDWorld.Reset();
    // 旧对象/租约都已摘下；移除委托若触发新HUD请求，旧栈只清自己的捕获值。
    if (HUD.IsValid()) HUD->RemoveFromParent();
    ReleaseArenaHUDLease(Pending, PendingInstance); ReleaseArenaHUDLease(Active, ActiveInstance);
}

void UDivineBeastsArenaUIClientSubsystem::RegisterArenaScreenDefinitions()
{
    if (!IsValid(PlatformUI))
    {
        return;
    }

    for (const FDivineBeastsArenaUISurfaceDescriptor& Surface :
         FDivineBeastsArenaUIScreenCatalog::GetSurfaces())
    {
        if (Surface.Kind != EDivineBeastsArenaUISurfaceKind::Screen ||
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
        Definition->PausePolicy = EGamePlatformUIPausePolicy::Never;
        Definition->Transition = EGamePlatformUITransition::Default;
        Definition->DefaultFocusWidgetName =
            Surface.DefaultFocusWidgetName;

        if (!Surface.MobileWidgetClassPath.IsEmpty())
        {
            const TSoftClassPtr<UGamePlatformUIScreen> MobileClass(
                FSoftObjectPath(Surface.MobileWidgetClassPath));

            // Android/iOS共享移动端结构变体；状态、ViewModel和Definition语义保持一致。
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

void UDivineBeastsArenaUIClientSubsystem::UnregisterArenaScreenDefinitions()
{
    if (IsValid(PlatformUI))
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

void UDivineBeastsArenaUIClientSubsystem::EnsureGameStateBinding(
    AGamePlatformArenaGameState* GameState)
{
    if (BoundGameState.Get() == GameState)
    {
        return;
    }

    UnbindArenaEvents();
    BoundGameState = GameState;

    if (!IsValid(GameState))
    {
        return;
    }

    GameState->OnArenaPlayersChanged.AddWeakLambda(this, [this]() { RefreshArenaViewFromWorld(); });
    GameState->OnArenaPhaseChanged.AddUObject(
        this,
        &UDivineBeastsArenaUIClientSubsystem::HandleArenaPhaseChanged);
    GameState->OnArenaTeamStatesChanged.AddUObject(
        this,
        &UDivineBeastsArenaUIClientSubsystem::HandleArenaTeamStatesChanged);
    GameState->OnArenaResultChanged.AddUObject(
        this,
        &UDivineBeastsArenaUIClientSubsystem::HandleArenaResultChanged);
}

void UDivineBeastsArenaUIClientSubsystem::EnsurePlayerStateBindings(
    const TArray<AGamePlatformArenaPlayerState*>& PlayerStates)
{
    bool bSameBindings =
        PlayerStates.Num() == BoundPlayerStates.Num();

    if (bSameBindings)
    {
        for (int32 Index = 0; Index < PlayerStates.Num(); ++Index)
        {
            if (BoundPlayerStates[Index].Get() != PlayerStates[Index])
            {
                bSameBindings = false;
                break;
            }
        }
    }

    if (bSameBindings)
    {
        return;
    }

    for (const TWeakObjectPtr<AGamePlatformArenaPlayerState>& PlayerPtr :
         BoundPlayerStates)
    {
        if (AGamePlatformArenaPlayerState* PlayerState = PlayerPtr.Get())
        {
            PlayerState->OnArenaStatsChanged.RemoveAll(this);
        }
    }

    BoundPlayerStates.Reset();
    BoundPlayerStates.Reserve(PlayerStates.Num());

    for (AGamePlatformArenaPlayerState* PlayerState : PlayerStates)
    {
        if (!IsValid(PlayerState))
        {
            continue;
        }

        PlayerState->OnArenaStatsChanged.AddUObject(
            this,
            &UDivineBeastsArenaUIClientSubsystem::HandleArenaPlayerStatsChanged);
        BoundPlayerStates.Add(PlayerState);
    }
}

void UDivineBeastsArenaUIClientSubsystem::UnbindArenaEvents()
{
    if (AGamePlatformArenaGameState* GameState = BoundGameState.Get())
    {
        GameState->OnArenaPlayersChanged.RemoveAll(this);
        GameState->OnArenaPhaseChanged.RemoveAll(this);
        GameState->OnArenaTeamStatesChanged.RemoveAll(this);
        GameState->OnArenaResultChanged.RemoveAll(this);
    }

    for (const TWeakObjectPtr<AGamePlatformArenaPlayerState>& PlayerPtr :
         BoundPlayerStates)
    {
        if (AGamePlatformArenaPlayerState* PlayerState = PlayerPtr.Get())
        {
            PlayerState->OnArenaStatsChanged.RemoveAll(this);
        }
    }

    BoundGameState.Reset();
    BoundPlayerStates.Reset();
}

bool UDivineBeastsArenaUIClientSubsystem::RefreshViewModelFromBoundState()
{
    AGamePlatformArenaGameState* GameState = BoundGameState.Get();
    if (!IsValid(GameState) || !IsValid(ArenaViewModel))
    {
        return false;
    }

    TArray<AGamePlatformArenaPlayerState*> PlayerStates;
    PlayerStates.Reserve(BoundPlayerStates.Num());
    for (const TWeakObjectPtr<AGamePlatformArenaPlayerState>& PlayerPtr :
         BoundPlayerStates)
    {
        if (AGamePlatformArenaPlayerState* PlayerState = PlayerPtr.Get())
        {
            PlayerStates.Add(PlayerState);
        }
    }

    ArenaViewModel->RefreshFromReplicatedState(
        GameState,
        PlayerStates);
    return true;
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaPhaseChanged(
    EGamePlatformArenaMatchPhase MatchPhase,
    int32 Revision)
{
    // 参数仅用于事件签名；ViewModel始终从GameState完整快照读取，避免事件增量状态漂移。
    RefreshArenaViewFromWorld();
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaTeamStatesChanged(
    const TArray<FGamePlatformArenaTeamState>& TeamStates)
{
    // 队伍变化可能伴随PlayerArray变化，因此重新核对一次玩家统计绑定。
    RefreshArenaViewFromWorld();
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaResultChanged(
    const FGamePlatformArenaResultSummary& Result)
{
    RefreshViewModelFromBoundState();
    SyncArenaSurface();
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaPlayerStatsChanged(
    AGamePlatformArenaPlayerState* PlayerState)
{
    // 统计更新只重建最多10人的只读记分板，不重新绑定委托。
    RefreshViewModelFromBoundState();
}
