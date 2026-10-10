#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaTypes.h"

class APawn;
/** 当前玩家的只读玩法拥有快照；Pawn由世界拥有，代次由出生适配器签发，不授予权威资格。 */
struct FGamePlatformArenaGameplayOwnership
{
    /** 借用受控Pawn。已准入重连尚未出生时可为空；调用方不得销毁或据此授权Active。 */
    TWeakObjectPtr<APawn> Pawn;
    /** 正数是本装配最后已签发的出生代次，0表示没有可捕获的身份；不是客户端自报代次。 */
    int32 AvatarGeneration = 0;
};

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
    /** 游戏线程只读捕获PlayerId当前受控Pawn/正代次；false清空OutOwnership，缺世界/连接/已出生代次即失败。
     * 成功不代表Ready/Active，不产生出生或权限副作用；重连尚无Pawn时保留原已签发代次。 */
    virtual bool CapturePlayerGameplayOwnership(const FString& PlayerId, FGamePlatformArenaGameplayOwnership& OutOwnership) const = 0;
    virtual bool SpawnPlayer(
        const FString& PlayerId,
        FName TeamId,
        FName SpawnPolicyId,
        FString& OutReason) = 0;
    /** 游戏线程权威设置当前Pawn资格；Active需完整Ready/ActorInfo/拥有关系，Inactive同时取消本玩家复活计时器。 */
    virtual bool SetPlayerGameplayActive(const FString& PlayerId, bool bActive, FString& OutReason) = 0;
    virtual bool RequestRespawn(
        const FString& PlayerId,
        FName TeamId,
        FName RespawnPolicyId,
        float DelaySeconds,
        FString& OutReason) = 0;
};
