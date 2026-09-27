#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaTypes.h"

/** IGamePlatformArenaScorePolicy（竞技评分策略接口）。仅消费服务器可信事件。 */
class IGamePlatformArenaScorePolicy
{
public:
    virtual ~IGamePlatformArenaScorePolicy() = default;
    virtual bool EvaluateScore(
        const FGamePlatformArenaTrustedEvent& Event,
        FGamePlatformArenaScoreDelta& OutDelta) const = 0;
};

/** IGamePlatformArenaWinConditionPolicy（竞技胜负策略接口）。 */
class IGamePlatformArenaWinConditionPolicy
{
public:
    virtual ~IGamePlatformArenaWinConditionPolicy() = default;
    virtual FGamePlatformArenaWinDecision EvaluateWinCondition(
        const TArray<FGamePlatformArenaTeamState>& Teams,
        const FGamePlatformArenaTrustedEvent& TriggerEvent) const = 0;
};

/** IGamePlatformArenaHeroEligibilityProvider（英雄竞技资格提供接口）。项目层服务器可实现，平台竞技层不依赖项目Entitlement客户端。 */
class IGamePlatformArenaHeroEligibilityProvider
{
public:
    virtual ~IGamePlatformArenaHeroEligibilityProvider() = default;
    virtual bool IsHeroEligible(
        const FString& PlayerId,
        const FString& HeroDefinitionId,
        FName ArenaModeId,
        FString& OutReason) const = 0;
};

/** IGamePlatformArenaGameplayLifecycleAdapter（竞技到Gameplay生命周期的服务器适配接口）。
 *  Arena只决定何时/哪一队出生或复活；实际Pawn创建、生命值和Character生命周期仍由Gameplay/Character权威实现。
 */
class IGamePlatformArenaGameplayLifecycleAdapter
{
public:
    virtual ~IGamePlatformArenaGameplayLifecycleAdapter() = default;
    virtual bool SpawnPlayer(
        const FString& PlayerId,
        FName TeamId,
        FName SpawnPolicyId,
        FString& OutReason) = 0;
    virtual bool RequestRespawn(
        const FString& PlayerId,
        FName TeamId,
        FName RespawnPolicyId,
        float DelaySeconds,
        FString& OutReason) = 0;
};
