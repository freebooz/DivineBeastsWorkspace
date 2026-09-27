#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Arena/GamePlatformArenaTypes.h"
#include "Client/GamePlatformArenaClientTypes.h"
#include "GamePlatformArenaViewModel.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;

/** FGamePlatformArenaScoreboardRow（客户端记分板只读行）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMARENACLIENT_API FGamePlatformArenaScoreboardRow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) FName TeamId;
    UPROPERTY(BlueprintReadOnly) FString HeroDefinitionId;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(BlueprintReadOnly) int32 Assists = 0;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
};

/** UGamePlatformArenaViewModel（竞技客户端只读视图模型）。
 *  不决定比赛规则、不直接修改GameState/PlayerState。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMARENACLIENT_API UGamePlatformArenaViewModel : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaClientFlowState FlowState = EGamePlatformArenaClientFlowState::Idle;
    UPROPERTY(BlueprintReadOnly) FString MatchId;
    UPROPERTY(BlueprintReadOnly) FName ArenaModeId;
    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaMatchPhase MatchPhase = EGamePlatformArenaMatchPhase::Uninitialized;
    UPROPERTY(BlueprintReadOnly) double RemainingPhaseSeconds = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformArenaTeamState> Teams;
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformArenaScoreboardRow> Scoreboard;
    UPROPERTY(BlueprintReadOnly) FGamePlatformArenaResultSummary ResultSummary;

    UFUNCTION(BlueprintCallable, Category="Arena")
    void RefreshFromReplicatedState(
        const AGamePlatformArenaGameState* GameState,
        const TArray<AGamePlatformArenaPlayerState*>& PlayerStates);
};
