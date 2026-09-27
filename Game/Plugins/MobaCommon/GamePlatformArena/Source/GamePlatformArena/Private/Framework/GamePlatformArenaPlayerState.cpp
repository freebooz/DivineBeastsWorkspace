#include "Framework/GamePlatformArenaPlayerState.h"

#include "Net/UnrealNetwork.h"

AGamePlatformArenaPlayerState::AGamePlatformArenaPlayerState()
{
    bReplicates = true;
}

void AGamePlatformArenaPlayerState::AuthorityApplyRosterIdentity(
    const FString& InPlayerId,
    const FString& InCharacterId,
    FName InTeamId)
{
    if (!HasAuthority() || InPlayerId.IsEmpty() || InCharacterId.IsEmpty() || InTeamId.IsNone()) { return; }
    PlayerIdPublic = InPlayerId;
    CharacterId = InCharacterId;
    TeamId = InTeamId;
    ConnectionState = EGamePlatformArenaConnectionState::Connected;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
}

bool AGamePlatformArenaPlayerState::AuthoritySelectHero(const FString& InHeroDefinitionId)
{
    if (!HasAuthority() || InHeroDefinitionId.IsEmpty() || !HeroDefinitionId.IsEmpty()) { return false; }
    HeroDefinitionId = InHeroDefinitionId;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
    return true;
}

bool AGamePlatformArenaPlayerState::AuthoritySetReady(bool bInReady)
{
    if (!HasAuthority() || HeroDefinitionId.IsEmpty()) { return false; }
    if (bReady == bInReady) { return true; }
    bReady = bInReady;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
    return true;
}

void AGamePlatformArenaPlayerState::AuthorityRecordKill() { if (HasAuthority()) { ++Kills; ++StatsRevision; ForceNetUpdate(); OnArenaStatsChanged.Broadcast(this); } }
void AGamePlatformArenaPlayerState::AuthorityRecordDeath() { if (HasAuthority()) { ++Deaths; ++StatsRevision; ForceNetUpdate(); OnArenaStatsChanged.Broadcast(this); } }
void AGamePlatformArenaPlayerState::AuthorityRecordAssist() { if (HasAuthority()) { ++Assists; ++StatsRevision; ForceNetUpdate(); OnArenaStatsChanged.Broadcast(this); } }

void AGamePlatformArenaPlayerState::AuthorityAddScore(int32 ScoreDelta, int32 ObjectiveDelta)
{
    if (!HasAuthority()) { return; }
    ArenaScore += ScoreDelta;
    ObjectiveScore += ObjectiveDelta;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
}

void AGamePlatformArenaPlayerState::AuthoritySetConnectionState(EGamePlatformArenaConnectionState NewState)
{
    if (!HasAuthority()) { return; }
    ConnectionState = NewState;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
}

void AGamePlatformArenaPlayerState::AuthoritySetForfeitState(EGamePlatformArenaForfeitState NewState)
{
    if (!HasAuthority()) { return; }
    ForfeitState = NewState;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
}

void AGamePlatformArenaPlayerState::OnRep_StatsRevision()
{
    OnArenaStatsChanged.Broadcast(this);
}

void AGamePlatformArenaPlayerState::AuthorityRestoreCompetitiveState(
    const FString& InHeroDefinitionId,
    bool bInReady,
    int32 InKills,
    int32 InDeaths,
    int32 InAssists,
    int32 InScore,
    int32 InObjectiveScore,
    EGamePlatformArenaForfeitState InForfeitState)
{
    if (!HasAuthority()) { return; }
    HeroDefinitionId = InHeroDefinitionId;
    bReady = bInReady;
    Kills = FMath::Max(0, InKills);
    Deaths = FMath::Max(0, InDeaths);
    Assists = FMath::Max(0, InAssists);
    ArenaScore = InScore;
    ObjectiveScore = InObjectiveScore;
    ForfeitState = InForfeitState;
    ConnectionState = EGamePlatformArenaConnectionState::Connected;
    ++StatsRevision;
    ForceNetUpdate();
    OnArenaStatsChanged.Broadcast(this);
}

void AGamePlatformArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, PlayerIdPublic);
    DOREPLIFETIME_CONDITION(AGamePlatformArenaPlayerState, CharacterId, COND_OwnerOnly);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, TeamId);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, HeroDefinitionId);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, bReady);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, Kills);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, Deaths);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, Assists);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, ArenaScore);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, ObjectiveScore);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, ConnectionState);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, ForfeitState);
    DOREPLIFETIME(AGamePlatformArenaPlayerState, StatsRevision);
}
