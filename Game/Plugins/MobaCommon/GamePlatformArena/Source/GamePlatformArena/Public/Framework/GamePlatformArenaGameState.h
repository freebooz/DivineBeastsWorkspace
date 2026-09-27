#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Arena/GamePlatformArenaTypes.h"
#include "GamePlatformArenaGameState.generated.h"

/** Arena复制事实的客户端原生事件；上层只读适配器可订阅，不形成Arena→Presentation依赖。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformArenaPhaseChangedNative,
    EGamePlatformArenaMatchPhase,
    int32);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformArenaTeamStatesChangedNative,
    const TArray<FGamePlatformArenaTeamState>&);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformArenaObjectiveStatesChangedNative,
    const TArray<FGamePlatformArenaObjectiveState>&);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformArenaResultChangedNative,
    const FGamePlatformArenaResultSummary&);

/** AGamePlatformArenaGameState（竞技公共游戏状态）。
 *  只复制公开比赛事实，不复制TransferTicket、后端凭据或隐藏匹配值。
 */
UCLASS()
class GAMEPLATFORMARENA_API AGamePlatformArenaGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AGamePlatformArenaGameState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FString MatchIdPublic;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FName ArenaModeId;
    UPROPERTY(ReplicatedUsing=OnRep_MatchPhase, BlueprintReadOnly, Category="Arena") EGamePlatformArenaMatchPhase MatchPhase = EGamePlatformArenaMatchPhase::Uninitialized;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 PhaseRevision = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") double PhaseStartServerTime = 0.0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") double PhaseDeadlineServerTime = 0.0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") double MatchStartServerTime = 0.0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") double MatchEndServerTime = 0.0;
    UPROPERTY(ReplicatedUsing=OnRep_TeamStates, BlueprintReadOnly, Category="Arena") TArray<FGamePlatformArenaTeamState> TeamStates;
    UPROPERTY(ReplicatedUsing=OnRep_ObjectiveStates, BlueprintReadOnly, Category="Arena") TArray<FGamePlatformArenaObjectiveState> ObjectiveStates;
    UPROPERTY(ReplicatedUsing=OnRep_ResultSummary, BlueprintReadOnly, Category="Arena") FGamePlatformArenaResultSummary ResultSummary;

    void AuthorityInitializeMatch(const FGamePlatformArenaAssignment& Assignment);
    void AuthoritySetPhase(EGamePlatformArenaMatchPhase NewPhase, int32 NewRevision, double DurationSeconds = 0.0);
    bool AuthorityAddTeamScore(FName TeamId, int32 ScoreDelta, int32 ObjectiveDelta = 0);
    void AuthoritySetResult(const FGamePlatformArenaResultSummary& Summary);

    UFUNCTION(BlueprintPure, Category="Arena") double GetPhaseRemainingSeconds() const;
    const FGamePlatformArenaTeamState* FindTeam(FName TeamId) const;

    FGamePlatformArenaPhaseChangedNative OnArenaPhaseChanged;
    FGamePlatformArenaTeamStatesChangedNative OnArenaTeamStatesChanged;
    FGamePlatformArenaObjectiveStatesChangedNative OnArenaObjectiveStatesChanged;
    FGamePlatformArenaResultChangedNative OnArenaResultChanged;

    UFUNCTION() void OnRep_MatchPhase();
    UFUNCTION() void OnRep_TeamStates();
    UFUNCTION() void OnRep_ObjectiveStates();
    UFUNCTION() void OnRep_ResultSummary();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
