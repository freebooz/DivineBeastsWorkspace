// 项目角色能力资格读取；双端游戏线程运行，服务器查实时GameMode，客户端只消费复制PlayerState。
#include "Abilities/DivineBeastsCharacterActivationGate.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Framework/GamePlatformGameModeBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

FDivineBeastsCharacterActivationGate::FDivineBeastsCharacterActivationGate(UDivineBeastsCharacterComponent& Component,
    int32 SpawnGeneration, int32 AvatarGeneration)
    : CharacterComponent(&Component), ExpectedSpawnGeneration(SpawnGeneration), ExpectedAvatarGeneration(AvatarGeneration) {}

FGamePlatformResult FDivineBeastsCharacterActivationGate::Evaluate(const UGamePlatformAbilitySystemComponent& Component) const
{
    check(IsInGameThread());
    const auto Failure = [](FName Code) { return FGamePlatformResult::Failure(Code, TEXT("当前角色或玩法权威资格不满足能力激活条件。")); };
    const auto* Character = CharacterComponent.Get();
    if (!Character || !Character->IsCharacterReady()) { return Failure(TEXT("CharacterNotReady")); }
    if (ExpectedSpawnGeneration <= 0 || ExpectedAvatarGeneration <= 0 ||
        Character->GetSpawnGeneration() != ExpectedSpawnGeneration || Character->GetAvatarGeneration() != ExpectedAvatarGeneration)
    { return Failure(TEXT("CharacterGenerationStale")); }
    const APawn* Pawn = Cast<APawn>(Character->GetOwner());
    const auto* World = Pawn ? Pawn->GetWorld() : nullptr;
    if (!Pawn || !World || World->bIsTearingDown || World != Component.GetWorld() || Component.GetAvatarActor() != Pawn)
    { return Failure(TEXT("CharacterAvatarMismatch")); }
    const auto* Controller = Cast<APlayerController>(Pawn->GetController());
    const auto* PlayerState = Controller ? Controller->GetPlayerState<AGamePlatformPlayerStateBase>() : nullptr;
    if (!Controller || Controller->GetPawn() != Pawn || !Controller->PlayerState || Controller->GetWorld() != World || Controller->PlayerState->GetWorld() != World)
    { return Failure(TEXT("GameplayOwnerUnavailable")); }
    const AActor* AbilityOwner = Component.GetOwnerActor();
    if (AbilityOwner != Pawn && AbilityOwner != Controller && AbilityOwner != Controller->PlayerState)
    { return Failure(TEXT("AbilityOwnerMismatch")); }
    const auto* Combat = Pawn->FindComponentByClass<UGamePlatformCombatComponent>();
    if (!Combat || Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != ExpectedAvatarGeneration)
    { return Failure(TEXT("CombatAvatarUnavailable")); }
    if (!PlayerState)
    {
        // 非平台Experience宿主（例如MOBA竞技）复用同一中立资格组件；资格只可由服务器出生/死亡/排空适配写入。
        const auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        if (!Eligibility || !Combat || !Eligibility->IsServerPlayerActiveForGameplay() ||
            Eligibility->GetGameplayAvatarGeneration() != ExpectedAvatarGeneration || Combat->IsCombatDead())
        { return Failure(TEXT("GameplayNotActive")); }
        if (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled()) { return Failure(TEXT("GameplayPredictionNotOwned")); }
        return FGamePlatformResult::Success();
    }
    const auto Snapshot = PlayerState->GetLifecycleSnapshot();
    // 项目初始化Context.AvatarGeneration对应可信玩法PawnGeneration；ASC自身绑定计数属于另一个作用域。
    if (!Snapshot.IsServerActive() || Snapshot.ControlledPawn != Pawn ||
        Snapshot.SpawnGeneration != ExpectedSpawnGeneration || Snapshot.PawnGeneration != ExpectedAvatarGeneration)
    { return Failure(TEXT("GameplayNotActive")); }
    if (Pawn->HasAuthority())
    {
        const auto* GameMode = World->GetAuthGameMode<AGamePlatformGameModeBase>();
        if (!GameMode || !GameMode->IsPlayerGameplayActive(*Controller)) { return Failure(TEXT("GameplayAuthorityDenied")); }
    }
    else if (!Pawn->IsLocallyControlled()) { return Failure(TEXT("GameplayPredictionNotOwned")); }
    return FGamePlatformResult::Success();
}
