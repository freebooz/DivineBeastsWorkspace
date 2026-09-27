#include "Framework/GamePlatformArenaPlayerController.h"

#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerState.h"

namespace
{
    AGamePlatformArenaGameMode* GetArenaMode(AGamePlatformArenaPlayerController* Controller)
    {
        return Controller != nullptr && Controller->GetWorld() != nullptr
            ? Controller->GetWorld()->GetAuthGameMode<AGamePlatformArenaGameMode>()
            : nullptr;
    }
}

void AGamePlatformArenaPlayerController::ServerRequestHeroSelection_Implementation(const FString& HeroDefinitionId)
{
    AGamePlatformArenaGameMode* Mode = GetArenaMode(this);
    AGamePlatformArenaPlayerState* State = GetPlayerState<AGamePlatformArenaPlayerState>();
    if (Mode == nullptr || State == nullptr)
    {
        ClientArenaRequestRejected(TEXT("竞技服务器状态尚未就绪。"));
        return;
    }
    FString Error;
    if (!Mode->RequestHeroSelection(State, HeroDefinitionId, Mode->GetHeroEligibilityProvider(), Error))
    {
        ClientArenaRequestRejected(Error);
    }
}

void AGamePlatformArenaPlayerController::ServerRequestReady_Implementation()
{
    AGamePlatformArenaGameMode* Mode = GetArenaMode(this);
    AGamePlatformArenaPlayerState* State = GetPlayerState<AGamePlatformArenaPlayerState>();
    FString Error;
    if (Mode == nullptr || State == nullptr || !Mode->RequestReady(State, Error))
    {
        ClientArenaRequestRejected(Error.IsEmpty() ? TEXT("Ready请求失败。") : Error);
    }
}

void AGamePlatformArenaPlayerController::ServerRequestForfeit_Implementation()
{
    AGamePlatformArenaGameMode* Mode = GetArenaMode(this);
    AGamePlatformArenaPlayerState* State = GetPlayerState<AGamePlatformArenaPlayerState>();
    FString Error;
    if (Mode == nullptr || State == nullptr || !Mode->RequestForfeit(State, Error))
    {
        ClientArenaRequestRejected(Error.IsEmpty() ? TEXT("弃权请求失败。") : Error);
    }
}

void AGamePlatformArenaPlayerController::ClientArenaRequestRejected_Implementation(const FString& Reason)
{
    UE_LOG(LogTemp, Warning, TEXT("Arena request rejected: %s"), *Reason);
}
