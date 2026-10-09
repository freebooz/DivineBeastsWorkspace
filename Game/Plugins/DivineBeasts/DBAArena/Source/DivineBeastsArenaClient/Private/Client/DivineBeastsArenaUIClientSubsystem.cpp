#include "Client/DivineBeastsArenaUIClientSubsystem.h"

// GetRootLayout（获取界面根布局）与GetParent（获取面板父控件）需要完整UObject派生类型，
// 仅有前置声明时IsValid无法安全地执行指针转换；此处显式引用对应公共头文件。
#include "Layers/GamePlatformUILayerStack.h"
#include "Components/PanelWidget.h"

#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
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

    Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformUI =
            LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    }

    ArenaViewModel = NewObject<UGamePlatformArenaViewModel>(this);
    RegisteredDefinitions.Reserve(5);
    BoundPlayerStates.Reserve(10);

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

    // ClientTravel / SeamlessTravel 后不再信任旧页面与世界HUD；后续只依据新世界事实装配。
    CloseArenaScreen();
    RemoveArenaHUD();
    FailedArenaSurfaceId = NAME_None;
    RefreshArenaViewFromWorld();
}

bool UDivineBeastsArenaUIClientSubsystem::RefreshArenaViewFromWorld()
{
    UWorld* World = GetWorld();
    AGamePlatformArenaGameState* GameState =
        World ? World->GetGameState<AGamePlatformArenaGameState>() : nullptr;
    if (!IsValid(GameState) || !IsValid(ArenaViewModel))
    {
        // 前端匹配阶段可以没有ArenaGameState，保留外部匹配适配器上报的流程状态。
        // 仅在确认离开上一竞技世界时清除旧复制状态，避免僵尸HUD残留。
        const bool bLeftArenaWorld = BoundGameState.IsValid();
        UnbindArenaEvents();
        if (bLeftArenaWorld && IsValid(ArenaViewModel))
        {
            ArenaViewModel->SetObservedClientFlowState(
                EGamePlatformArenaClientFlowState::Idle);
        }
        SyncArenaSurface();
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
    SyncArenaSurface();
    return true;
}

FName UDivineBeastsArenaUIClientSubsystem::ResolvePrimaryArenaSurfaceId() const
{
    if (!IsValid(ArenaViewModel))
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
    if (IsValid(ArenaViewModel))
    {
        ArenaViewModel->SetObservedClientFlowState(InFlowState);
        SyncArenaSurface();
    }
}


void UDivineBeastsArenaUIClientSubsystem::SyncArenaSurface()
{
    if (!IsValid(PlatformUI) || !IsValid(ArenaViewModel))
    {
        return;
    }

    const FName Desired = ResolvePrimaryArenaSurfaceId();
    if (FailedArenaSurfaceId != Desired)
    {
        // 状态切换后才允许重新检查曾缺失的资源，禁止复制事件导致无限尝试。
        FailedArenaSurfaceId = NAME_None;
    }
    if (Desired.IsNone())
    {
        CloseArenaScreen();
        RemoveArenaHUD();
        return;
    }

    if (Desired == TEXT("UI.HUD.Arena"))
    {
        CloseArenaScreen();
        EnsureArenaHUD();
        return;
    }

    RemoveArenaHUD();
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
    bDispatchingArenaScreenOpen = false;

    // 平台可能同步拒绝或者立即完成命中缓存的加载；这些情况不能留下僵尸请求。
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
    OpeningArenaRequestId.Invalidate();
    OpeningArenaSurfaceId = NAME_None;
    bDispatchingArenaScreenOpen = false;
    bArenaScreenOpenFailedDuringDispatch = false;
    bArenaScreenOpenedDuringDispatch = false;

    if (IsValid(PlatformUI) && RequestId.IsValid())
    {
        PlatformUI->CancelOpen(RequestId);
    }

    UGamePlatformUIScreen* Screen = ActiveArenaScreen.Get();
    ActiveArenaScreen.Reset();
    ActiveArenaSurfaceId = NAME_None;
    if (IsValid(PlatformUI) && IsValid(Screen))
    {
        // 只关闭本子系统打开的页面，不触碰DBAClient公共登录与世界页面。
        PlatformUI->CloseScreen(Screen);
    }
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpened(
    FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen)
{
    const bool bExpectedRequest =
        (OpeningArenaRequestId.IsValid() && OpeningArenaRequestId == RequestId) ||
        (bDispatchingArenaScreenOpen && OpeningArenaSurfaceId == ScreenId);
    if (!bExpectedRequest || !IsValid(Screen))
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

    UGamePlatformUIScreen* Previous = ActiveArenaScreen.Get();
    ActiveArenaScreen = Screen;
    ActiveArenaSurfaceId = ScreenId;
    FailedArenaSurfaceId = NAME_None;
    if (IsValid(Previous) && Previous != Screen && IsValid(PlatformUI))
    {
        PlatformUI->CloseScreen(Previous);
    }
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenOpenFailed(
    FGuid RequestId, FName ScreenId, FText Reason)
{
    const bool bExpectedRequest =
        (OpeningArenaRequestId.IsValid() && OpeningArenaRequestId == RequestId) ||
        (bDispatchingArenaScreenOpen && OpeningArenaSurfaceId == ScreenId);
    if (!bExpectedRequest)
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
    if (ScreenId == ActiveArenaSurfaceId)
    {
        ActiveArenaScreen.Reset();
        ActiveArenaSurfaceId = NAME_None;
    }
}

void UDivineBeastsArenaUIClientSubsystem::EnsureArenaHUD()
{
    if (!IsValid(PlatformUI) || !IsValid(PlatformUI->GetRootLayout()) ||
        !IsValid(ArenaViewModel) ||
        FailedArenaSurfaceId == TEXT("UI.HUD.Arena"))
    {
        return;
    }
    if (IsValid(ActiveArenaHUD) && IsValid(ActiveArenaHUD->GetParent()))
    {
        return;
    }
    if (PendingArenaHUDLoad.IsValid())
    {
        return;
    }

    const FDivineBeastsArenaUISurfaceDescriptor* Surface =
        FDivineBeastsArenaUIScreenCatalog::Find(TEXT("UI.HUD.Arena"));
    if (!Surface || Surface->WidgetClassPath.IsEmpty())
    {
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        return;
    }

    const FSoftObjectPath HUDClassPath(Surface->WidgetClassPath);
    if (!HUDClassPath.IsValid())
    {
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        return;
    }

    const uint32 RequestGeneration = ++ArenaHUDRequestGeneration;
    // UI资源异步加载，避免从匹配切到实时战斗时同步磁盘加载造成长帧。
    PendingArenaHUDLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        HUDClassPath,
        FStreamableDelegate::CreateUObject(
            this,
            &UDivineBeastsArenaUIClientSubsystem::HandleArenaHUDLoaded,
            RequestGeneration));
    if (!PendingArenaHUDLoad.IsValid())
    {
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
    }
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaHUDLoaded(
    uint32 RequestGeneration)
{
    if (RequestGeneration != ArenaHUDRequestGeneration ||
        ResolvePrimaryArenaSurfaceId() != TEXT("UI.HUD.Arena") ||
        !IsValid(PlatformUI) || !IsValid(PlatformUI->GetRootLayout()))
    {
        return;
    }

    const FDivineBeastsArenaUISurfaceDescriptor* Surface =
        FDivineBeastsArenaUIScreenCatalog::Find(TEXT("UI.HUD.Arena"));
    TSoftClassPtr<UGamePlatformMobaArenaHUDBase> HUDClass(
        FSoftObjectPath(Surface ? Surface->WidgetClassPath : FString()));
    UClass* LoadedClass = HUDClass.Get();
    APlayerController* PlayerController = GetLocalPlayer()
        ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;

    if (!IsValid(LoadedClass) ||
        LoadedClass->HasAnyClassFlags(CLASS_Abstract) ||
        !LoadedClass->IsChildOf(UGamePlatformMobaArenaHUDBase::StaticClass()) ||
        !IsValid(PlayerController))
    {
        PendingArenaHUDLoad.Reset();
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        UE_LOG(LogTemp, Warning, TEXT("DBAArena HUD资源不存在或父类不合法，等待真实Monolith资产交付。"));
        return;
    }

    UGamePlatformMobaArenaHUDBase* HUD = CreateWidget<UGamePlatformMobaArenaHUDBase>(
        PlayerController, LoadedClass);
    if (!IsValid(HUD))
    {
        PendingArenaHUDLoad.Reset();
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        return;
    }

    HUD->InitializeArenaViewModel(ArenaViewModel);
    if (!PlatformUI->AttachHUDWidget(HUD))
    {
        HUD->RemoveFromParent();
        PendingArenaHUDLoad.Reset();
        FailedArenaSurfaceId = TEXT("UI.HUD.Arena");
        return;
    }

    ActiveArenaHUD = HUD;
    ActiveArenaHUDLease = MoveTemp(PendingArenaHUDLoad);
    FailedArenaSurfaceId = NAME_None;
}

void UDivineBeastsArenaUIClientSubsystem::RemoveArenaHUD()
{
    ++ArenaHUDRequestGeneration;
    if (PendingArenaHUDLoad.IsValid())
    {
        PendingArenaHUDLoad->CancelHandle();
        PendingArenaHUDLoad.Reset();
    }
    if (IsValid(ActiveArenaHUD))
    {
        ActiveArenaHUD->RemoveFromParent();
    }
    ActiveArenaHUD = nullptr;
    ActiveArenaHUDLease.Reset();
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
