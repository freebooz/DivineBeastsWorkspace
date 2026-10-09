// 项目客户端每LocalPlayer竞技UI适配：借用平台页面/Data与MOBA事实，拥有World/Actor委托；退出清空投影，无Widget轮询。
#include "Client/DivineBeastsArenaUIClientSubsystem.h"

#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "UI/DivineBeastsArenaUIScreenCatalog.h"
#include "ViewModels/GamePlatformArenaViewModel.h"

void UDivineBeastsArenaUIClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bDeinitializing = false;

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
    RegisterArenaScreenDefinitions();
    RefreshArenaViewFromWorld();
}

void UDivineBeastsArenaUIClientSubsystem::Deinitialize()
{
    bDeinitializing = true;
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    BindWorldReadinessEvents(nullptr);
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

    // ClientTravel / SeamlessTravel 后 PlayerController 或 World 可能改变；
    // 重新解析当前竞技GameState，不保留旧世界对象引用。
    RefreshArenaViewFromWorld();
}

bool UDivineBeastsArenaUIClientSubsystem::RefreshArenaViewFromWorld()
{
    if (bDeinitializing) { return false; }
    UWorld* World = GetWorld();
    if (World && (World->bIsTearingDown || RetiredWorld.Get() == World)) { World = nullptr; }
    BindWorldReadinessEvents(World);
    AGamePlatformArenaGameState* GameState =
        World ? World->GetGameState<AGamePlatformArenaGameState>() : nullptr;
    if (!IsValid(GameState) || !IsValid(ArenaViewModel))
    {
        const bool bHadArenaWorld = BoundGameState.IsValid();
        UnbindArenaEvents();
        if (IsValid(ArenaViewModel) && (bHadArenaWorld || !ArenaViewModel->MatchId.IsEmpty()))
        { ArenaViewModel->ResetReplicatedArenaState(); }
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
    return true;
}

void UDivineBeastsArenaUIClientSubsystem::BindWorldReadinessEvents(UWorld* World)
{
    if (BoundWorld.Get() == World) { return; }
    if (BoundWorld.IsValid()) { BoundWorld->GameStateSetEvent.Remove(GameStateSetHandle); }
    // 跨世界先失效旧投影；保持新的匹配/连接流程由其拥有者后续事件更新。
    const bool bResetPreviousWorld = BoundWorld.IsValid();
    UnbindArenaEvents();
    BoundWorld = World; GameStateSetHandle.Reset();
    if (World)
    { GameStateSetHandle = World->GameStateSetEvent.AddWeakLambda(this, [this](AGameStateBase*) { RefreshArenaViewFromWorld(); }); }
    if (bResetPreviousWorld && IsValid(ArenaViewModel)) { ArenaViewModel->ResetReplicatedArenaState(); }
}
void UDivineBeastsArenaUIClientSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
    if (BoundWorld.Get() != World) { return; }
    RetiredWorld = World;
    BindWorldReadinessEvents(nullptr);
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
    }
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
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaPlayerStatsChanged(
    AGamePlatformArenaPlayerState* PlayerState)
{
    // 统计更新只重建最多10人的只读记分板，不重新绑定委托。
    RefreshViewModelFromBoundState();
}
