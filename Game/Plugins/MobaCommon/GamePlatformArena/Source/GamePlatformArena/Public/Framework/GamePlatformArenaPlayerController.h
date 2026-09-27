#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GamePlatformArenaPlayerController.generated.h"

/** AGamePlatformArenaPlayerController（竞技玩家控制器）。
 *  只暴露低频、自身范围请求：选英雄、Ready、弃权；没有SetTeam/SetScore/SetWinner/SubmitResult入口。
 */
UCLASS()
class GAMEPLATFORMARENA_API AGamePlatformArenaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(Server, Reliable)
    void ServerRequestHeroSelection(const FString& HeroDefinitionId);

    UFUNCTION(Server, Reliable)
    void ServerRequestReady();

    UFUNCTION(Server, Reliable)
    void ServerRequestForfeit();

    UFUNCTION(Client, Reliable)
    void ClientArenaRequestRejected(const FString& Reason);
};
