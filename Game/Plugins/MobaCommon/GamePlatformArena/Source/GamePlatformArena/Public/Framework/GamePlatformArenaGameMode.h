#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Arena/GamePlatformArenaPolicies.h"
#include "Definitions/GamePlatformArenaModeDefinition.h"
#include "State/GamePlatformArenaMatchStateMachine.h"
#include "GamePlatformArenaGameMode.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;

/** FGamePlatformArenaResultPendingDelegate（比赛结果待提交事件）。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformArenaResultPendingDelegate,
    const FGamePlatformArenaMatchResult&);

/** AGamePlatformArenaGameMode（MainArena服务器权威竞技规则入口）。 */
UCLASS()
class GAMEPLATFORMARENA_API AGamePlatformArenaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGamePlatformArenaGameMode();

    UPROPERTY(EditDefaultsOnly, Category="Arena") float CountdownSeconds = 5.0f;
    UPROPERTY(EditDefaultsOnly, Category="Arena") float ReconnectGracePeriodSeconds = 90.0f;
    UPROPERTY(EditDefaultsOnly, Category="Arena") int32 StandardKillScore = 1;
    UPROPERTY(EditDefaultsOnly, Category="Arena") int32 StandardWinScore = 10;
    UPROPERTY(EditDefaultsOnly, Category="Arena") float StandardRespawnDelaySeconds = 5.0f;
    UPROPERTY(EditDefaultsOnly, Category="Arena") float HeroSelectionTimeoutSeconds = 30.0f;
    UPROPERTY(EditDefaultsOnly, Category="Arena") float ReadyCheckTimeoutSeconds = 15.0f;

    virtual void BeginPlay() override;
    virtual void Logout(AController* Exiting) override;

    bool ApplyAssignment(const FGamePlatformArenaAssignment& Assignment, FString& OutError);
    bool ApplyAssignmentWithModeSpec(
        const FGamePlatformArenaAssignment& Assignment,
        const FGamePlatformArenaModeSpec& ModeSpec,
        FString& OutError);
    bool AdmitPlayer(
        AGamePlatformArenaPlayerState* PlayerState,
        const FGamePlatformArenaTransferTicketClaims& VerifiedTicket,
        FString& OutError);
    bool RequestHeroSelection(
        AGamePlatformArenaPlayerState* PlayerState,
        const FString& HeroDefinitionId,
        const IGamePlatformArenaHeroEligibilityProvider* EligibilityProvider,
        FString& OutError);
    bool RequestReady(AGamePlatformArenaPlayerState* PlayerState, FString& OutError);
    bool HandleTrustedEvent(const FGamePlatformArenaTrustedEvent& Event, FString& OutError);
    bool RequestForfeit(AGamePlatformArenaPlayerState* PlayerState, FString& OutError);
    void MarkPlayerDisconnected(const FString& PlayerId);
    bool IsPlayerAwaitingReconnect(const FString& PlayerId) const;
    bool TryReconnectPlayer(AGamePlatformArenaPlayerState* NewPlayerState, const FString& PlayerId, FString& OutError);
    bool EndMatch(FName WinningTeamId, EGamePlatformArenaMatchEndReason Reason, FString& OutError);
    bool MarkResultCommitted(FString& OutError);

    void SetHeroEligibilityProvider(const IGamePlatformArenaHeroEligibilityProvider* InProvider) { HeroEligibilityProvider = InProvider; }
    const IGamePlatformArenaHeroEligibilityProvider* GetHeroEligibilityProvider() const { return HeroEligibilityProvider; }
    void SetGameplayLifecycleAdapter(IGamePlatformArenaGameplayLifecycleAdapter* InAdapter) { GameplayLifecycleAdapter = InAdapter; }
    bool HasGameplayLifecycleAdapter() const { return GameplayLifecycleAdapter != nullptr; }

    const FGamePlatformArenaAssignment& GetAssignment() const { return CurrentAssignment; }
    const FGamePlatformArenaMatchResult& GetPendingResult() const { return PendingResult; }
    const FGamePlatformArenaModeSpec& GetModeSpec() const { return ActiveModeSpec; }
    FGamePlatformArenaResultPendingDelegate& OnResultPending() { return ResultPendingDelegate; }

private:
    struct FPlayerRecoveryState
    {
        FString HeroDefinitionId;
        bool bReady = false;
        int32 Kills = 0;
        int32 Deaths = 0;
        int32 Assists = 0;
        int32 Score = 0;
        int32 ObjectiveScore = 0;
        EGamePlatformArenaForfeitState ForfeitState = EGamePlatformArenaForfeitState::None;
    };

    bool Transition(EGamePlatformArenaMatchPhase NewPhase, FString& OutError, double DurationSeconds = 0.0);
    bool ValidateAssignment(const FGamePlatformArenaAssignment& Assignment, const FGamePlatformArenaModeSpec& Mode, FString& OutError) const;
    bool AreAllPlayersAdmitted() const;
    bool AreAllPlayersSelected() const;
    bool AreAllPlayersReady() const;
    void BeginHeroSelectionIfReady();
    void BeginReadyCheckIfReady();
    void BeginCountdownIfReady();
    void StartMatchFromCountdown();
    void HandlePreMatchTimeout();
    void HandleMatchTimeLimit();
    void HandleReconnectTimeout(const FString PlayerId);
    FName GetOpponentTeam(FName TeamId) const;
    void BuildPendingResult(FName WinningTeamId, EGamePlatformArenaMatchEndReason Reason);

    FGamePlatformArenaMatchStateMachine PhaseMachine;
    FGamePlatformArenaAssignment CurrentAssignment;
    FGamePlatformArenaModeSpec ActiveModeSpec;
    TMap<FString, TWeakObjectPtr<AGamePlatformArenaPlayerState>> PlayerStatesById;
    TMap<FString, FPlayerRecoveryState> RecoveryStatesById;
    TMap<FString, FTimerHandle> ReconnectTimers;
    TSet<FString> ProcessedEventIds;
    FGamePlatformArenaMatchResult PendingResult;
    FDateTime MatchStartedAtUtc;
    const IGamePlatformArenaHeroEligibilityProvider* HeroEligibilityProvider = nullptr;
    IGamePlatformArenaGameplayLifecycleAdapter* GameplayLifecycleAdapter = nullptr;
    FTimerHandle CountdownTimer;
    FTimerHandle PreMatchTimeoutTimer;
    FTimerHandle MatchDeadlineTimer;
    FGamePlatformArenaResultPendingDelegate ResultPendingDelegate;
    bool bAssignmentApplied = false;
    bool bEndStarted = false;
};
