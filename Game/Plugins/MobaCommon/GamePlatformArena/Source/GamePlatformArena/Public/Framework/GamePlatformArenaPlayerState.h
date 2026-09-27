#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Arena/GamePlatformArenaTypes.h"
#include "GamePlatformArenaPlayerState.generated.h"

class AGamePlatformArenaPlayerState;

/** 参赛者复制统计变化事件；用于客户端只读表现/记分板适配。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformArenaPlayerStatsChangedNative,
    AGamePlatformArenaPlayerState*);

/** AGamePlatformArenaPlayerState（参赛者公开竞技状态）。 */
UCLASS()
class GAMEPLATFORMARENA_API AGamePlatformArenaPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AGamePlatformArenaPlayerState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FString PlayerIdPublic;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FString CharacterId;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FName TeamId;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") FString HeroDefinitionId;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") bool bReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 Kills = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 Deaths = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 Assists = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 ArenaScore = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") int32 ObjectiveScore = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") EGamePlatformArenaConnectionState ConnectionState = EGamePlatformArenaConnectionState::PendingAdmission;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Arena") EGamePlatformArenaForfeitState ForfeitState = EGamePlatformArenaForfeitState::None;
    UPROPERTY(ReplicatedUsing=OnRep_StatsRevision, BlueprintReadOnly, Category="Arena") int32 StatsRevision = 0;

    void AuthorityApplyRosterIdentity(const FString& InPlayerId, const FString& InCharacterId, FName InTeamId);
    bool AuthoritySelectHero(const FString& InHeroDefinitionId);
    bool AuthoritySetReady(bool bInReady);
    void AuthorityRecordKill();
    void AuthorityRecordDeath();
    void AuthorityRecordAssist();
    void AuthorityAddScore(int32 ScoreDelta, int32 ObjectiveDelta = 0);
    void AuthoritySetConnectionState(EGamePlatformArenaConnectionState NewState);
    void AuthoritySetForfeitState(EGamePlatformArenaForfeitState NewState);
    void AuthorityRestoreCompetitiveState(
        const FString& InHeroDefinitionId,
        bool bInReady,
        int32 InKills,
        int32 InDeaths,
        int32 InAssists,
        int32 InScore,
        int32 InObjectiveScore,
        EGamePlatformArenaForfeitState InForfeitState);

    FGamePlatformArenaPlayerStatsChangedNative OnArenaStatsChanged;

    UFUNCTION() void OnRep_StatsRevision();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
