// 每竞技世界的权威出生/复活适配：Pawn唯一拥有组件，成功发布Active前验证Definition、ActorInfo和Combat；退出取消本世界计时器。
#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Initialization/GamePlatformCharacterInitializationExecutor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Types/GamePlatformCombatEvent.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

namespace
{
    constexpr float MaximumArenaRespawnDelaySeconds = 600.0f;
}

FDivineBeastsArenaGameplayLifecycleAdapter::FDivineBeastsArenaGameplayLifecycleAdapter(
    AGamePlatformArenaGameMode& InGameMode, const TArray<FGamePlatformDataLease>& WarmupLeases)
    : GameMode(&InGameMode), DefinitionWarmupLeases(WarmupLeases)
{
}

FDivineBeastsArenaGameplayLifecycleAdapter::~FDivineBeastsArenaGameplayLifecycleAdapter()
{
    if (auto* Mode = GameMode.Get())
    {
        for (const auto& Slot : Mode->GetAssignment().Roster) { FString Ignored; SetPlayerGameplayActive(Slot.PlayerId, false, Ignored); }
    }
    TArray<FString> DeathPlayers; DeathBindings.GetKeys(DeathPlayers);
    for (const auto& PlayerId : DeathPlayers) { UnbindPawnDeath(PlayerId); }
    ClearRespawnTimers();
    // 角色借用装配预热资源，必须先结束角色/自有需求，再由上层释放World预热需求。
    for (const auto& Pair : SpawnedPawns) { if (Pair.Value.IsValid()) { Pair.Value->Destroy(); } }
    SpawnedPawns.Reset(); DefinitionWarmupLeases.Reset();
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::ResolvePlayer(
    const FString& PlayerId,
    AGamePlatformArenaPlayerController*& OutController,
    AGamePlatformArenaPlayerState*& OutPlayerState,
    FString& OutReason) const
{
    OutController = nullptr;
    OutPlayerState = nullptr;

    AGamePlatformArenaGameMode* Mode = GameMode.Get();
    UWorld* World = Mode ? Mode->GetWorld() : nullptr;
    if (!Mode || !World || !Mode->HasAuthority() || PlayerId.IsEmpty())
    {
        OutReason = TEXT("竞技角色出生目标世界或PlayerId无效。");
        return false;
    }

    for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
         Iterator;
         ++Iterator)
    {
        AGamePlatformArenaPlayerController* Controller =
            Cast<AGamePlatformArenaPlayerController>(Iterator->Get());
        AGamePlatformArenaPlayerState* State =
            Controller
                ? Controller->GetPlayerState<AGamePlatformArenaPlayerState>()
                : nullptr;
        if (State && State->PlayerIdPublic == PlayerId &&
            State->ConnectionState == EGamePlatformArenaConnectionState::Connected)
        {
            OutController = Controller;
            OutPlayerState = State;
            break;
        }
    }

    if (!OutController || !OutPlayerState)
    {
        OutReason = TEXT("未找到与PlayerId绑定的当前竞技Controller/PlayerState。");
        return false;
    }
    if (OutPlayerState->CharacterId.IsEmpty() ||
        OutPlayerState->HeroDefinitionId.IsEmpty() ||
        OutPlayerState->TeamId.IsNone())
    {
        OutReason = TEXT("竞技玩家缺少可信CharacterId、HeroDefinitionId或TeamId。");
        return false;
    }
    if (OutPlayerState->ConnectionState != EGamePlatformArenaConnectionState::Connected)
    {
        OutReason = TEXT("竞技玩家当前连接状态不允许出生。");
        return false;
    }

    OutReason.Reset();
    return true;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::CapturePlayerGameplayOwnership(
    const FString& PlayerId, FGamePlatformArenaGameplayOwnership& OutOwnership) const
{
    check(IsInGameThread()); OutOwnership = {};
    AGamePlatformArenaPlayerController* Controller = nullptr; AGamePlatformArenaPlayerState* State = nullptr; FString Error;
    auto* Mode = GameMode.Get(); auto* World = Mode ? Mode->GetWorld() : nullptr;
    if (!Mode || !World || World->bIsTearingDown || !Mode->IsGameplayLifecycleAdapter(this, Mode->GetGameplayLifecycleBindingId()) ||
        !ResolvePlayer(PlayerId, Controller, State, Error)) { return false; }
    const int32 Generation = SpawnGenerations.FindRef(PlayerId);
    if (Generation <= 0) { return false; }
    APawn* Pawn = Controller->GetPawn();
    if (Pawn && (SpawnedPawns.FindRef(PlayerId).Get() != Pawn || Pawn->GetWorld() != World || Pawn->GetController() != Controller ||
        !Pawn->FindComponentByClass<UGamePlatformCombatComponent>() ||
        Pawn->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatAvatarGeneration() != Generation)) { return false; }
    // 已准入死亡重连尚无Pawn时，捕获原最后已签发代次；该快照不代表Ready或允许Active。
    OutOwnership.Pawn = Pawn; OutOwnership.AvatarGeneration = Generation; return true;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::IsSpawnScopeCurrent(const FString& PlayerId,
    TWeakObjectPtr<AGamePlatformArenaPlayerController> Controller, TWeakObjectPtr<AGamePlatformArenaPlayerState> State,
    TWeakObjectPtr<APawn> Pawn, FString MatchId, FString ServerId, FGuid BindingId,
    EGamePlatformArenaMatchPhase Phase, int32 Generation) const
{
    auto* Mode = GameMode.Get(); auto* World = Mode ? Mode->GetWorld() : nullptr;
    if (!Mode || !World || World->bIsTearingDown || !Mode->HasAuthority() || !Controller.IsValid() || !State.IsValid() ||
        (Phase != EGamePlatformArenaMatchPhase::Countdown && Phase != EGamePlatformArenaMatchPhase::InProgress) ||
        Mode->GetMatchPhase() != Phase || !Mode->IsGameplayLifecycleAdapter(this, BindingId) || MatchId.IsEmpty() ||
        Mode->GetAssignment().MatchId != MatchId || Mode->GetAssignment().GameServerId != ServerId ||
        Controller->GetWorld() != World || State->GetWorld() != World || Controller->GetPlayerState<AGamePlatformArenaPlayerState>() != State.Get() ||
        Controller->GetPawn() != Pawn.Get() || (Pawn.IsValid() && Pawn->GetController() != Controller.Get()) || State->ConnectionState != EGamePlatformArenaConnectionState::Connected ||
        State->PlayerIdPublic != PlayerId || SpawnGenerations.FindRef(PlayerId) != Generation) { return false; }
    const auto* ArenaState = Mode->GetGameState<AGamePlatformArenaGameState>();
    if (!ArenaState || ArenaState->GetWorld() != World || ArenaState->MatchIdPublic != MatchId || ArenaState->MatchPhase != Phase) { return false; }
    int32 MatchingSlots = 0;
    for (const auto& Slot : Mode->GetAssignment().Roster)
    { if (Slot.PlayerId == PlayerId && Slot.CharacterId == State->CharacterId && Slot.TeamId == State->TeamId && Slot.IsValid()) { ++MatchingSlots; } }
    return MatchingSlots == 1;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::SpawnPlayer(
    const FString& PlayerId,
    FName TeamId,
    FName SpawnPolicyId,
    FString& OutReason)
{
    check(IsInGameThread());

    if (TeamId.IsNone() || SpawnPolicyId.IsNone())
    {
        OutReason = TEXT("竞技出生必须提供TeamId和SpawnPolicyId。");
        return false;
    }

    AGamePlatformArenaPlayerController* Controller = nullptr;
    AGamePlatformArenaPlayerState* State = nullptr;
    if (!ResolvePlayer(PlayerId, Controller, State, OutReason))
    {
        return false;
    }
    auto* Mode = GameMode.Get();
    const FString ExpectedMatchId = Mode ? Mode->GetAssignment().MatchId : FString();
    const FString ExpectedServerId = Mode ? Mode->GetAssignment().GameServerId : FString();
    const FGuid ExpectedBindingId = Mode ? Mode->GetGameplayLifecycleBindingId() : FGuid();
    const auto ExpectedPhase = Mode ? Mode->GetMatchPhase() : EGamePlatformArenaMatchPhase::Uninitialized;
    const TWeakObjectPtr<AGamePlatformArenaPlayerController> ExpectedController(Controller);
    const TWeakObjectPtr<AGamePlatformArenaPlayerState> ExpectedState(State);
    TWeakObjectPtr<APawn> ExpectedPawn(Controller->GetPawn()); int32 ExpectedGeneration = SpawnGenerations.FindRef(PlayerId);
    const auto IsCurrent = [&]() { return IsSpawnScopeCurrent(PlayerId, ExpectedController, ExpectedState, ExpectedPawn,
        ExpectedMatchId, ExpectedServerId, ExpectedBindingId, ExpectedPhase, ExpectedGeneration); };
    if (!IsCurrent() || (SpawnPolicyId != Mode->GetModeSpec().SpawnPolicyId &&
        (ExpectedPhase != EGamePlatformArenaMatchPhase::InProgress || SpawnPolicyId != Mode->GetModeSpec().RespawnPolicyId)))
    { OutReason = TEXT("出生原世界/比赛/装配/阶段/连接上下文已撤销。"); return false; }
    if (State->TeamId != TeamId)
    {
        OutReason = TEXT("出生请求TeamId与服务器Roster身份不一致。");
        return false;
    }

    const FName HeroDefinitionId(*State->HeroDefinitionId);
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
    {
        OutReason = TEXT("出生请求HeroDefinitionId不属于十二生肖核心Catalog。");
        return false;
    }

    // Match开始阶段禁止冷加载：Assignment配置时已经预热全部Server-safe Hero Definition。
    const FSoftObjectPath DefinitionPath =
        FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroDefinitionId);
    const UDivineBeastsHeroDefinition* LoadedDefinition =
        DefinitionPath.IsValid()
            ? Cast<UDivineBeastsHeroDefinition>(DefinitionPath.ResolveObject())
            : nullptr;
    if (!LoadedDefinition)
    {
        OutReason = TEXT("Hero Definition尚未完成服务器预热，拒绝进入比赛出生。");
        return false;
    }

    // 优先使用TeamId标记的PlayerStart；未配置时退回UE标准ChoosePlayerStart路径。
    AActor* StartSpot = Mode->FindPlayerStart(Controller, TeamId.ToString());
    if (!IsCurrent()) { OutReason = TEXT("出生点解析期间原比赛已撤销。"); return false; }
    if (!StartSpot)
    {
        StartSpot = Mode->FindPlayerStart(Controller, SpawnPolicyId.ToString());
    }
    if (!IsCurrent()) { OutReason = TEXT("出生点解析期间原比赛已撤销。"); return false; }
    if (!StartSpot)
    {
        StartSpot = Mode->FindPlayerStart(Controller, FString());
    }
    if (!IsCurrent() || !IsValid(StartSpot) || StartSpot->GetWorld() != Mode->GetWorld())
    {
        OutReason = TEXT("原比赛已撤销或未找到有效PlayerStart出生点。");
        return false;
    }

    UnbindPawnDeath(PlayerId);
    if (APawn* ExistingPawn = ExpectedPawn.Get()) { ExistingPawn->Destroy(); }
    ExpectedPawn.Reset();
    if (!IsCurrent()) { OutReason = TEXT("旧Pawn退出后原出生上下文已撤销。"); return false; }
    // 项目Pawn组合平台ASC/Combat/资格默认子组件；生肖差异仍由Definition/外观配置表达。
    TGuardValue<TSubclassOf<APawn>> PawnClassGuard(
        Mode->DefaultPawnClass,
        ADivineBeastsCharacter::StaticClass());
    Mode->RestartPlayerAtPlayerStart(Controller, StartSpot);

    ACharacter* Character = ExpectedController.IsValid() ? Cast<ACharacter>(ExpectedController->GetPawn()) : nullptr;
    ExpectedPawn = Character;
    if (!IsCurrent())
    { if (Character && ExpectedController.IsValid() && ExpectedController->GetPlayerState<AGamePlatformArenaPlayerState>() == ExpectedState.Get()) { Character->Destroy(); }
      OutReason = TEXT("Restart返回后原比赛/连接已撤销。"); return false; }
    if (!Character)
    {
        OutReason = TEXT("标准GameMode出生入口未生成ACharacter。");
        return false;
    }

    if (ExpectedGeneration >= MAX_int32)
    { Character->Destroy(); OutReason = TEXT("角色SpawnGeneration已达到安全上限。"); return false; }
    // 保存值而非跨初始化广播借用TMap元素，其他玩家重入出生可能使Map重新分配。
    const int32 SpawnGeneration = ++ExpectedGeneration;
    SpawnGenerations.Add(PlayerId, SpawnGeneration);

    FGamePlatformCharacterInitializationContext Context;
    Context.CharacterId = State->CharacterId;
    Context.HeroDefinitionId = HeroDefinitionId;
    Context.SpawnGeneration = SpawnGeneration;
    Context.AvatarGeneration = SpawnGeneration;
    Context.bPersistentCharacterIdRequired = true;

    if (!FGamePlatformCharacterInitializationExecutor::InitializeCharacter(
            *Character,
            Context,
            OutReason))
    {
        Character->Destroy();
        return false;
    }

    if (!IsCurrent()) { Character->Destroy(); OutReason = TEXT("角色初始化广播撤销了原比赛。"); return false; }
    UDivineBeastsCharacterComponent* CharacterState =
        Character->FindComponentByClass<UDivineBeastsCharacterComponent>();
    const FGamePlatformDataLease* WarmupLease = DefinitionWarmupLeases.FindByPredicate(
        [&DefinitionPath](const auto& Lease) { return Lease.ResourcePaths.Contains(DefinitionPath); });
    if (CharacterState && !CharacterState->IsCharacterReady() && WarmupLease)
    { CharacterState->TryUsePreloadedDefinition(*WarmupLease, *Mode, OutReason); }
    if (!IsCurrent() || !CharacterState || !CharacterState->IsCharacterReady())
    {
        Character->Destroy();
        OutReason = TEXT("项目角色初始化尚未达到Ready，拒绝进入比赛。");
        return false;
    }

    SpawnedPawns.Add(PlayerId, Character);
    auto* ASC = Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    auto* Combat = Character->FindComponentByClass<UGamePlatformCombatComponent>();
    auto* Eligibility = Character->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
    if (!ASC || !Combat || !Combat->GetCombatAttributeSet() || !Eligibility || ASC->GetAvatarActor() != Character ||
        !Eligibility->BindServerAvatarGeneration(SpawnGeneration) || Combat->GetCombatAvatarGeneration() != SpawnGeneration)
    { Character->Destroy(); OutReason = TEXT("真实角色ASC/Combat/ActorInfo/玩法代次尚未完整初始化。"); return false; }
    if (auto* ProjectPawn = Cast<ADivineBeastsCharacter>(Character)) { BindPawnDeath(PlayerId, *ProjectPawn, *State, SpawnGeneration); }
    // 初次出生属于倒计时事务；全部Pawn成功后由GameMode统一激活。局内复活则必须仍处于InProgress。
    if (!IsCurrent())
    { UnbindPawnDeath(PlayerId); Character->Destroy(); OutReason = TEXT("激活前原出生上下文已撤销。"); return false; }
    if (ExpectedPhase == EGamePlatformArenaMatchPhase::InProgress) { Eligibility->SetServerPlayerActive(true); }
    if (!IsCurrent())
    { Eligibility->SetServerPlayerActive(false); UnbindPawnDeath(PlayerId); Character->Destroy();
      OutReason = TEXT("玩法激活后原出生上下文已撤销。"); return false; }
    OutReason.Reset();
    return true;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::SetPlayerGameplayActive(const FString& PlayerId, bool bActive, FString& OutReason)
{
    check(IsInGameThread());
    // 即使本装配已被替换，仍只能清理自己的Timer/订阅；资格修改必须另行取得当前绑定和自有Pawn。
    if (!bActive)
    {
        UnbindPawnDeath(PlayerId);
        if (const FRespawnRequest* Request = RespawnTimers.Find(PlayerId))
        {
            // UE ClearTimer会使传入句柄失效；只提供本请求已确认的可变副本，不修改另一代请求。
            FTimerHandle OwnedTimer = Request->Timer;
            if (UWorld* OwnerWorld = Request->World.Get()) { OwnerWorld->GetTimerManager().ClearTimer(OwnedTimer); }
            RespawnTimers.Remove(PlayerId);
        }
    }
    auto* Mode = GameMode.Get(); auto* World = Mode ? Mode->GetWorld() : nullptr;
    const FGuid BindingId = Mode ? Mode->GetGameplayLifecycleBindingId() : FGuid();
    const FString MatchId = Mode ? Mode->GetAssignment().MatchId : FString();
    const TWeakObjectPtr<APawn> OwnedPawn = SpawnedPawns.FindRef(PlayerId);
    const int32 Generation = SpawnGenerations.FindRef(PlayerId);
    if (!World || !Mode->HasAuthority() || !Mode->IsGameplayLifecycleAdapter(this, BindingId) || !OwnedPawn.IsValid() || Generation <= 0)
    { OutReason = TEXT("本装配已撤销或缺少自有Pawn/代次，不能修改其他装配玩法资格。"); return false; }
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        auto* Controller = Cast<AGamePlatformArenaPlayerController>(It->Get());
        auto* State = Controller ? Controller->GetPlayerState<AGamePlatformArenaPlayerState>() : nullptr;
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (!State || State->PlayerIdPublic != PlayerId || Pawn != OwnedPawn.Get()) { continue; }
        const TWeakObjectPtr<AGamePlatformArenaPlayerController> ExpectedController(Controller);
        const TWeakObjectPtr<AGamePlatformArenaPlayerState> ExpectedState(State);
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        auto* Character = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        auto* ASC = Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
        auto* Combat = Pawn->FindComponentByClass<UGamePlatformCombatComponent>();
        const auto IsOwnedContextCurrent = [&]()
        {
            if (!GameMode.IsValid() || GameMode.Get() != Mode || Mode->GetWorld() != World ||
                !Mode->IsGameplayLifecycleAdapter(this, BindingId) || Mode->GetAssignment().MatchId != MatchId ||
                !ExpectedController.IsValid() || !ExpectedState.IsValid() || !OwnedPawn.IsValid() ||
                SpawnedPawns.FindRef(PlayerId) != OwnedPawn || SpawnGenerations.FindRef(PlayerId) != Generation ||
                ExpectedController->GetPawn() != OwnedPawn.Get() || OwnedPawn->GetController() != ExpectedController.Get() ||
                ExpectedController->GetPlayerState<AGamePlatformArenaPlayerState>() != ExpectedState.Get() ||
                OwnedPawn->GetPlayerState<AGamePlatformArenaPlayerState>() != ExpectedState.Get() ||
                ExpectedState->GetOwner() != ExpectedController.Get() || ExpectedState->GetWorld() != World ||
                ExpectedState->PlayerIdPublic != PlayerId || !IsValid(Eligibility) || !IsValid(Combat) ||
                Eligibility->GetGameplayAvatarGeneration() != Generation || Combat->GetCombatAvatarGeneration() != Generation) { return false; }
            int32 Slots = 0;
            for (const auto& Slot : Mode->GetAssignment().Roster)
            { if (Slot.PlayerId == PlayerId && Slot.CharacterId == ExpectedState->CharacterId && Slot.TeamId == ExpectedState->TeamId && Slot.IsValid()) { ++Slots; } }
            return Slots == 1;
        };
        if (!IsOwnedContextCurrent() || (bActive && (World->bIsTearingDown || Mode->GetMatchPhase() != EGamePlatformArenaMatchPhase::InProgress ||
            !Character || !Character->IsCharacterReady() || !ASC || ASC->GetAvatarActor() != Pawn || Combat->IsCombatDead() ||
            State->ConnectionState != EGamePlatformArenaConnectionState::Connected)))
        { OutReason = TEXT("当前Controller/PlayerState/Pawn/代次不属于本装配或不满足激活前提。"); return false; }
        Eligibility->SetServerPlayerActive(bActive);
        // 资格通知也允许同步结束/换装配，返回后不能继续承诺旧拥有关系有效。
        if (!IsOwnedContextCurrent()) { OutReason = TEXT("资格广播已撤销原拥有上下文。"); return false; }
        OutReason.Reset(); return true;
    }
    OutReason = TEXT("未找到本装配自有且当前受控的Roster Pawn。"); return false;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::CaptureRespawnContext(
    const FString& PlayerId, FName TeamId, FName PolicyId, FRespawnRequest& OutContext) const
{
    OutContext = {};
    auto* Mode = GameMode.Get(); auto* World = Mode ? Mode->GetWorld() : nullptr;
    AGamePlatformArenaPlayerController* Controller = nullptr; AGamePlatformArenaPlayerState* State = nullptr; FString Error;
    FGamePlatformArenaGameplayOwnership Ownership;
    if (!Mode || !World || World->bIsTearingDown || !Mode->HasAuthority() ||
        Mode->GetMatchPhase() != EGamePlatformArenaMatchPhase::InProgress || PolicyId != Mode->GetModeSpec().RespawnPolicyId ||
        !ResolvePlayer(PlayerId, Controller, State, Error) || State->TeamId != TeamId || !CapturePlayerGameplayOwnership(PlayerId, Ownership)) { return false; }
    OutContext.RequestId = FGuid::NewGuid(); OutContext.BindingId = Mode->GetGameplayLifecycleBindingId();
    OutContext.World = World; OutContext.Controller = Controller; OutContext.State = State; OutContext.Pawn = Ownership.Pawn;
    OutContext.bHadPawn = Ownership.Pawn.IsValid(); OutContext.MatchId = Mode->GetAssignment().MatchId;
    OutContext.ServerId = Mode->GetAssignment().GameServerId; OutContext.CharacterId = State->CharacterId;
    OutContext.TeamId = TeamId; OutContext.PolicyId = PolicyId; OutContext.AvatarGeneration = Ownership.AvatarGeneration;
    return IsRespawnContextCurrent(PlayerId, OutContext);
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::IsRespawnContextCurrent(
    const FString& PlayerId, const FRespawnRequest& Context) const
{
    auto* Mode = GameMode.Get(); auto* World = Context.World.Get();
    if (!Mode || !World || World != Mode->GetWorld() || World->bIsTearingDown || !Context.RequestId.IsValid() ||
        !Mode->IsGameplayLifecycleAdapter(this, Context.BindingId) || Mode->GetMatchPhase() != EGamePlatformArenaMatchPhase::InProgress ||
        Mode->GetAssignment().MatchId != Context.MatchId || Mode->GetAssignment().GameServerId != Context.ServerId ||
        Mode->GetModeSpec().RespawnPolicyId != Context.PolicyId || !Context.Controller.IsValid() || !Context.State.IsValid() ||
        Context.Controller->GetPlayerState<AGamePlatformArenaPlayerState>() != Context.State.Get() ||
        Context.State->CharacterId != Context.CharacterId || Context.State->TeamId != Context.TeamId ||
        Context.Controller->GetPawn() != Context.Pawn.Get() || (Context.bHadPawn && !Context.Pawn.IsValid()) ||
        Context.AvatarGeneration <= 0) { return false; }
    FGamePlatformArenaGameplayOwnership Ownership;
    return IsSpawnScopeCurrent(PlayerId, Context.Controller, Context.State, Context.Pawn, Context.MatchId, Context.ServerId,
        Context.BindingId, EGamePlatformArenaMatchPhase::InProgress, Context.AvatarGeneration) &&
        CapturePlayerGameplayOwnership(PlayerId, Ownership) && Ownership.Pawn == Context.Pawn && Ownership.AvatarGeneration == Context.AvatarGeneration;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::RequestRespawn(
    const FString& PlayerId, FName TeamId, FName RespawnPolicyId, float DelaySeconds, FString& OutReason)
{
    check(IsInGameThread());
    FRespawnRequest Context;
    if (PlayerId.IsEmpty() || TeamId.IsNone() || RespawnPolicyId.IsNone() || !FMath::IsFinite(DelaySeconds) ||
        DelaySeconds < 0.0f || DelaySeconds > MaximumArenaRespawnDelaySeconds || !CaptureRespawnContext(PlayerId, TeamId, RespawnPolicyId, Context))
    { OutReason = TEXT("竞技复活参数或原世界/比赛/玩家/Pawn代次已失效。"); return false; }
    UWorld* World = Context.World.Get();
    if (const FRespawnRequest* Existing = RespawnTimers.Find(PlayerId))
    {
        if (IsRespawnContextCurrent(PlayerId, *Existing) && World->GetTimerManager().IsTimerActive(Existing->Timer))
        { OutReason.Reset(); return true; }
        FTimerHandle OwnedTimer = Existing->Timer;
        World->GetTimerManager().ClearTimer(OwnedTimer); RespawnTimers.Remove(PlayerId);
    }
    const TWeakPtr<FDivineBeastsArenaGameplayLifecycleAdapter> WeakThis = AsShared();
    const FGuid RequestId = Context.RequestId;
    const FTimerDelegate Completion = FTimerDelegate::CreateLambda([WeakThis, PlayerId, RequestId]()
    { if (const auto Self = WeakThis.Pin()) { Self->ExecuteDeferredRespawn(PlayerId, RequestId); } });
    // 零秒也退出Combat广播栈；捕获身份不能在下一Tick授权已经终局、断线或换Pawn的新状态。
    if (DelaySeconds <= KINDA_SMALL_NUMBER) { Context.Timer = World->GetTimerManager().SetTimerForNextTick(Completion); }
    else { World->GetTimerManager().SetTimer(Context.Timer, Completion, DelaySeconds, false); }
    RespawnTimers.Add(PlayerId, Context); OutReason.Reset(); return true;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::ExecuteDeferredRespawn(FString PlayerId, FGuid ExpectedRequestId)
{
    const FRespawnRequest* Found = RespawnTimers.Find(PlayerId);
    // 第一项检查必须是请求身份，过期回调不得删除、清Timer或借用另一代请求的上下文。
    if (!Found || Found->RequestId != ExpectedRequestId) { return false; }
    const FRespawnRequest Context = *Found;
    RespawnTimers.Remove(PlayerId);
    FTimerHandle OwnedTimer = Context.Timer;
    if (UWorld* World = Context.World.Get()) { World->GetTimerManager().ClearTimer(OwnedTimer); }
    if (!IsRespawnContextCurrent(PlayerId, Context)) { return false; }
    FString Error;
    if (!SpawnPlayer(PlayerId, Context.TeamId, Context.PolicyId, Error))
    { UE_LOG(LogTemp, Error, TEXT("Arena respawn failed for player %s: %s"), *PlayerId, *Error); return false; }
    return true;
}

void FDivineBeastsArenaGameplayLifecycleAdapter::ClearRespawnTimers()
{
    // Mode可能已经EndPlay/弱引用失效，仍按各请求自有World清理，不能依赖当前Mode来找到旧Timer。
    for (auto& Pair : RespawnTimers)
    { if (UWorld* OwnerWorld = Pair.Value.World.Get()) { OwnerWorld->GetTimerManager().ClearTimer(Pair.Value.Timer); } }
    RespawnTimers.Reset();
}

void FDivineBeastsArenaGameplayLifecycleAdapter::UnbindPawnDeath(const FString& PlayerId)
{
    if (const FDeathBinding* Binding = DeathBindings.Find(PlayerId))
    { if (auto* Pawn = Binding->Pawn.Get()) { Pawn->OnAuthoritativeDeath().Remove(Binding->Handle); } }
    DeathBindings.Remove(PlayerId);
}

void FDivineBeastsArenaGameplayLifecycleAdapter::BindPawnDeath(const FString& PlayerId,
    ADivineBeastsCharacter& Pawn, AGamePlatformArenaPlayerState& State, int32 AvatarGeneration)
{
    UnbindPawnDeath(PlayerId);
    const TWeakPtr<FDivineBeastsArenaGameplayLifecycleAdapter> WeakThis = AsShared();
    const TWeakObjectPtr<ADivineBeastsCharacter> WeakPawn(&Pawn);
    const TWeakObjectPtr<AGamePlatformArenaPlayerState> WeakState(&State);
    const FString MatchId = GameMode.IsValid() ? GameMode->GetAssignment().MatchId : FString();
    FDeathBinding Binding; Binding.Pawn = &Pawn;
    Binding.Handle = Pawn.OnAuthoritativeDeath().AddLambda([WeakThis, PlayerId, WeakPawn, WeakState, MatchId, AvatarGeneration](const FGamePlatformCombatEvent& Event)
    { if (const auto Self = WeakThis.Pin()) { Self->HandlePawnDeath(PlayerId, WeakPawn, WeakState, MatchId, AvatarGeneration, Event); } });
    DeathBindings.Add(PlayerId, Binding);
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::HandlePawnDeath(const FString& PlayerId,
    TWeakObjectPtr<ADivineBeastsCharacter> Pawn, TWeakObjectPtr<AGamePlatformArenaPlayerState> State,
    FString MatchId, int32 AvatarGeneration, const FGamePlatformCombatEvent& Event)
{
    check(IsInGameThread());
    auto* Mode = GameMode.Get(); auto* World = Mode ? Mode->GetWorld() : nullptr;
    auto* Target = Pawn.Get(); auto* PlayerState = State.Get();
    auto* ArenaState = Mode ? Mode->GetGameState<AGamePlatformArenaGameState>() : nullptr;
    if (!Mode || !World || World->bIsTearingDown || !Mode->HasAuthority() || !Target || !PlayerState ||
        Target->GetWorld() != World || PlayerState->GetWorld() != World || MatchId.IsEmpty() ||
        Mode->GetAssignment().MatchId != MatchId || !ArenaState || ArenaState->GetWorld() != World || ArenaState->MatchIdPublic != MatchId ||
        ArenaState->MatchPhase != EGamePlatformArenaMatchPhase::InProgress ||
        !DeathBindings.Contains(PlayerId) || DeathBindings.FindChecked(PlayerId).Pawn.Get() != Target ||
        SpawnedPawns.FindRef(PlayerId).Get() != Target || SpawnGenerations.FindRef(PlayerId) != AvatarGeneration ||
        Event.EventType != EGamePlatformCombatEventType::Death || Event.TargetActor != Target || !Event.EventId.IsValid() ||
        Event.TargetAvatarGeneration != AvatarGeneration || !Target->GetGamePlatformCombatComponent()->IsCombatDead() ||
        Target->GetGamePlatformCombatComponent()->GetCombatAvatarGeneration() != AvatarGeneration) { return false; }
    AGamePlatformArenaPlayerController* Controller = nullptr; AGamePlatformArenaPlayerState* CurrentState = nullptr; FString Error;
    if (!ResolvePlayer(PlayerId, Controller, CurrentState, Error) || CurrentState != PlayerState || Controller->GetPawn() != Target) { return false; }
    const auto& Assignment = Mode->GetAssignment(); int32 MatchingSlots = 0;
    for (const auto& Slot : Assignment.Roster)
    { if (Slot.PlayerId == PlayerId && Slot.CharacterId == PlayerState->CharacterId && Slot.TeamId == PlayerState->TeamId && Slot.IsValid()) { ++MatchingSlots; } }
    if (MatchingSlots != 1) { return false; }
    FGamePlatformArenaTrustedEvent Trusted; Trusted.EventId = Event.EventId.ToString(EGuidFormats::DigitsWithHyphens);
    Trusted.EventType = EGamePlatformArenaTrustedEventType::Death; Trusted.PlayerId = PlayerId;
    // Combat源Actor并不等于已批准击杀归因，死亡桥接不能猜击杀、助攻或胜利规则。
    if (!Mode->HandleTrustedEvent(Trusted, Error))
    { UE_LOG(LogTemp, Error, TEXT("Arena authoritative death rejected: %s"), *Error); return false; }
    return true;
}
