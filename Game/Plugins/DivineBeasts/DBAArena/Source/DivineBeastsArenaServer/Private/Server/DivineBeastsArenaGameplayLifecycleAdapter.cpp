#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Initialization/GamePlatformCharacterInitializationExecutor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

namespace
{
    constexpr float MaximumArenaRespawnDelaySeconds = 600.0f;
}

FDivineBeastsArenaGameplayLifecycleAdapter::FDivineBeastsArenaGameplayLifecycleAdapter(
    AGamePlatformArenaGameMode& InGameMode)
    : GameMode(&InGameMode)
{
}

FDivineBeastsArenaGameplayLifecycleAdapter::~FDivineBeastsArenaGameplayLifecycleAdapter()
{
    ClearRespawnTimers();
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
        if (State && State->PlayerIdPublic == PlayerId)
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

    AGamePlatformArenaGameMode* Mode = GameMode.Get();
    if (!Mode)
    {
        OutReason = TEXT("Arena GameMode已失效。");
        return false;
    }

    if (APawn* ExistingPawn = Controller->GetPawn())
    {
        ExistingPawn->Destroy();
    }

    // 优先使用TeamId标记的PlayerStart；未配置时退回UE标准ChoosePlayerStart路径。
    AActor* StartSpot = Mode->FindPlayerStart(Controller, TeamId.ToString());
    if (!StartSpot)
    {
        StartSpot = Mode->FindPlayerStart(Controller, SpawnPolicyId.ToString());
    }
    if (!StartSpot)
    {
        StartSpot = Mode->FindPlayerStart(Controller, FString());
    }
    if (!StartSpot)
    {
        OutReason = TEXT("未找到可用PlayerStart出生点。");
        return false;
    }

    // 统一使用已有三层内的项目可玩角色：内含一个 ASC、可信英雄身份组件和技能授权组件，
    // 角色外观仍由项目表现包替换；不得让原型 ACharacter 缺少 GAS 而伪称技能可用。
    TGuardValue<TSubclassOf<APawn>> PawnClassGuard(
        Mode->DefaultPawnClass,
        ADivineBeastsGameplayCharacter::StaticClass());
    Mode->RestartPlayerAtPlayerStart(Controller, StartSpot);

    ACharacter* Character = Cast<ACharacter>(Controller->GetPawn());
    if (!Character)
    {
        OutReason = TEXT("标准GameMode出生入口未生成ACharacter。");
        return false;
    }

    int32& SpawnGeneration = SpawnGenerations.FindOrAdd(PlayerId);
    if (SpawnGeneration >= MAX_int32)
    {
        Character->Destroy();
        OutReason = TEXT("角色SpawnGeneration已达到安全上限。");
        return false;
    }
    ++SpawnGeneration;

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

    const UDivineBeastsCharacterComponent* CharacterState =
        Character->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!CharacterState || !CharacterState->IsCharacterReady())
    {
        Character->Destroy();
        OutReason = TEXT("项目角色初始化尚未达到Ready，拒绝进入比赛。");
        return false;
    }

    OutReason.Reset();
    return true;
}

bool FDivineBeastsArenaGameplayLifecycleAdapter::RequestRespawn(
    const FString& PlayerId,
    FName TeamId,
    FName RespawnPolicyId,
    float DelaySeconds,
    FString& OutReason)
{
    check(IsInGameThread());

    AGamePlatformArenaGameMode* Mode = GameMode.Get();
    UWorld* World = Mode ? Mode->GetWorld() : nullptr;
    if (!Mode || !World || PlayerId.IsEmpty() || TeamId.IsNone() ||
        RespawnPolicyId.IsNone() || !FMath::IsFinite(DelaySeconds) ||
        DelaySeconds < 0.0f || DelaySeconds > MaximumArenaRespawnDelaySeconds)
    {
        OutReason = TEXT("竞技复活请求参数或世界状态无效。");
        return false;
    }

    if (DelaySeconds <= KINDA_SMALL_NUMBER)
    {
        return SpawnPlayer(PlayerId, TeamId, RespawnPolicyId, OutReason);
    }

    if (FTimerHandle* ExistingHandle = RespawnTimers.Find(PlayerId))
    {
        if (World->GetTimerManager().IsTimerActive(*ExistingHandle))
        {
            // 同一玩家同一时刻只允许一个复活任务；重复死亡事件保持幂等。
            OutReason.Reset();
            return true;
        }
        RespawnTimers.Remove(PlayerId);
    }

    const TWeakPtr<FDivineBeastsArenaGameplayLifecycleAdapter> WeakThis = AsShared();
    FTimerHandle& Handle = RespawnTimers.Add(PlayerId);
    World->GetTimerManager().SetTimer(
        Handle,
        FTimerDelegate::CreateLambda(
            [WeakThis, PlayerId, TeamId, RespawnPolicyId]()
            {
                if (const TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter> Self =
                        WeakThis.Pin())
                {
                    Self->ExecuteDeferredRespawn(
                        PlayerId,
                        TeamId,
                        RespawnPolicyId);
                }
            }),
        DelaySeconds,
        false);

    OutReason.Reset();
    return true;
}

void FDivineBeastsArenaGameplayLifecycleAdapter::ExecuteDeferredRespawn(
    FString PlayerId,
    FName TeamId,
    FName RespawnPolicyId)
{
    RespawnTimers.Remove(PlayerId);

    FString Error;
    // RespawnPolicyId用于本轮策略身份；出生点仍优先按TeamId匹配，找不到时走标准PlayerStart。
    if (!SpawnPlayer(PlayerId, TeamId, RespawnPolicyId, Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Arena respawn failed for player %s: %s"),
            *PlayerId,
            *Error);
    }
}

void FDivineBeastsArenaGameplayLifecycleAdapter::ClearRespawnTimers()
{
    AGamePlatformArenaGameMode* Mode = GameMode.Get();
    UWorld* World = Mode ? Mode->GetWorld() : nullptr;
    if (World)
    {
        for (TPair<FString, FTimerHandle>& Pair : RespawnTimers)
        {
            World->GetTimerManager().ClearTimer(Pair.Value);
        }
    }
    RespawnTimers.Reset();
}
