#include "Framework/GamePlatformPlayerControllerBase.h"

#include "Framework/GamePlatformGameModeBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h" // 非Unity客户端目标需要UWorld完整类型来解析权威GameMode。

bool AGamePlatformPlayerControllerBase::IsServerPlayerActiveForGameplay() const
{
    return GetGameplayAvatarGeneration() > 0;
}

int32 AGamePlatformPlayerControllerBase::GetGameplayAvatarGeneration() const
{
    check(IsInGameThread());
    const UWorld* World = GetWorld();
    const APawn* ControlledPawn = GetPawn();
    const auto* State = GetPlayerState<AGamePlatformPlayerStateBase>();
    if (!HasAuthority() || IsActorBeingDestroyed() || !World || World->bIsTearingDown ||
        !IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed() || ControlledPawn->GetWorld() != World ||
        ControlledPawn->GetController() != this || !IsValid(State) || State->GetWorld() != World)
    {
        return 0;
    }
    const auto Snapshot = State->GetLifecycleSnapshot();
    if (!Snapshot.IsServerActive() || Snapshot.ControlledPawn != ControlledPawn ||
        Snapshot.PawnGeneration <= 0 || Snapshot.PawnGeneration > MAX_int32)
    {
        return 0;
    }
    // Experience宿主的唯一准入读取器已经校验当前连接与真实Active记录；
    // 不把该状态镜像到供其他宿主组合的Pawn资格组件，也不接受本地准备作为授权。
    const auto* Mode = World->GetAuthGameMode<AGamePlatformGameModeBase>();
    return Mode && Mode->IsPlayerGameplayActive(*this) ? static_cast<int32>(Snapshot.PawnGeneration) : 0;
}

FGamePlatformResult AGamePlatformPlayerControllerBase::ReportLocalPreparation(const FGamePlatformLocalPreparationFacts& Facts)
{
    check(IsInGameThread());

    if (!IsLocalController() || !PreparationToken.IsValid() || !Facts.IsComplete())
    {
        return FGamePlatformResult::Failure(
            TEXT("LocalPreparationInvalid"),
            TEXT("本地准备事实、拥有者身份或当前准备令牌无效。"));
    }

    const AGamePlatformPlayerStateBase* State = GetPlayerState<AGamePlatformPlayerStateBase>();
    const APawn* CurrentPawn = GetPawn();
    if (!State || !CurrentPawn)
    {
        return FGamePlatformResult::Failure(
            TEXT("LocalPreparationPawnUnavailable"),
            TEXT("当前拥有者尚未解析服务器认可的PlayerState或Pawn。"));
    }

    const FGamePlatformPlayerLifecycleSnapshot Snapshot = State->GetLifecycleSnapshot();
    if (Snapshot.Stage != EGamePlatformPlayerStage::AwaitingClient ||
        Snapshot.ControlledPawn != CurrentPawn ||
        Snapshot.PlayerGeneration != PreparationToken.PlayerGeneration ||
        Snapshot.PawnGeneration != PreparationToken.PawnGeneration ||
        Snapshot.StateRevision != PreparationToken.StateRevision)
    {
        return FGamePlatformResult::Failure(
            TEXT("LocalPreparationGenerationMismatch"),
            TEXT("本地准备事实与服务器复制的当前玩家/Pawn代次不一致。"));
    }

    if (LastSubmittedToken == PreparationToken.TokenId)
    {
        return FGamePlatformResult::Success();
    }

    LastSubmittedToken = PreparationToken.TokenId;
    ServerReportPrepared(PreparationToken);
    return FGamePlatformResult::Success();
}

void AGamePlatformPlayerControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AGamePlatformPlayerControllerBase, PreparationToken, COND_OwnerOnly);
}

void AGamePlatformPlayerControllerBase::OnRep_PreparationToken()
{
    // 新令牌或清空令牌都允许下一次本地组合根重新提交，避免旧TokenId抑制新Pawn。
    if (LastSubmittedToken != PreparationToken.TokenId)
    {
        LastSubmittedToken.Invalidate();
    }
    OnPreparationChanged.Broadcast();
}

void AGamePlatformPlayerControllerBase::ServerReportPrepared_Implementation(FGamePlatformPreparationToken Token)
{
    if (AGamePlatformGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGamePlatformGameModeBase>() : nullptr)
    {
        Mode->AcceptPreparation(*this, Token);
    }
}

void AGamePlatformPlayerControllerBase::SetPreparationToken(const FGamePlatformPreparationToken& Value)
{
    if (!HasAuthority())
    {
        return;
    }

    PreparationToken = Value;
    LastSubmittedToken.Invalidate();
    ForceNetUpdate();
}
