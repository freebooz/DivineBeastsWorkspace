#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformSpawnRequest.generated.h"

class AActor;
class AController;
class APawn;
class UWorld;

/** 世界、玩家和生成代次组成的出生幂等键；对象指针和显示名不参与身份。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformSpawnOperationId
{
    GENERATED_BODY()

    /** 当前GamePlatformWorld运行身份。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGuid WorldContextGeneration;

    /** 当前玩家记录代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 PlayerGeneration = 0;

    /** 当前服务器授权出生代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 SpawnGeneration = 0;

    bool IsValid() const
    {
        return WorldContextGeneration.IsValid() && PlayerGeneration > 0 && SpawnGeneration > 0;
    }

    bool operator==(const FGamePlatformSpawnOperationId& Other) const
    {
        return WorldContextGeneration == Other.WorldContextGeneration
            && PlayerGeneration == Other.PlayerGeneration && SpawnGeneration == Other.SpawnGeneration;
    }
};

/** 服务器生成策略的只读输入；Controller必须是当前世界真实准入记录的控制器。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformSpawnRequest
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AController> Controller;
    FGamePlatformSpawnOperationId OperationId;
    FGamePlatformId ExperienceId;
    FPrimaryAssetId PawnDefinitionId;
    FName SpawnPolicyId;
};

/**
 * 服务器策略返回的候选。Source必须属于当前世界，Transform单位厘米并保持有限；
 * 策略只提出候选，GameMode仍执行世界、区域、占位及Pawn碰撞复核。
 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformSpawnCandidate
{
    /** 当前策略内稳定候选键；同世界内不得重复。 */
    FName CandidateId;
    /** 低值优先，之后按CandidateId确定性排序。 */
    int32 Priority = 0;
    /** 候选来源，如APlayerStart；弱引用失效时拒绝。 */
    TWeakObjectPtr<AActor> Source;
    /** 实际生成变换，位置单位厘米。 */
    FTransform Transform = FTransform::Identity;
    /** 可选区域逻辑身份；非空时必须匹配当前已验证世界区域。 */
    FGamePlatformId RegionId;

    bool IsStructurallyValid(const UWorld& ExpectedWorld) const;
};

/** 服务器出生资格；Allowed只表示可以尝试候选，不表示已经生成或控制。 */
UENUM(BlueprintType)
enum class EGamePlatformSpawnEligibility : uint8
{
    Denied,
    Waiting,
    Allowed
};

/** 出生资格值结果；ReasonCode为脱敏稳定码，RetryAfterSeconds单位秒且只在Waiting有意义。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformSpawnEligibility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    EGamePlatformSpawnEligibility State = EGamePlatformSpawnEligibility::Denied;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName ReasonCode = TEXT("NotEvaluated");

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(Units="s"))
    float RetryAfterSeconds = 0.f;
};

/** 单次出生的脱敏结果；Pawn指针仅在服务器成功且对象仍存活时可用。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformSpawnResult
{
    FGamePlatformSpawnOperationId OperationId;
    TWeakObjectPtr<APawn> Pawn;
    FName CandidateId;
    FGamePlatformResult Result;
};

FORCEINLINE uint32 GetTypeHash(const FGamePlatformSpawnOperationId& Value)
{
    return HashCombine(GetTypeHash(Value.WorldContextGeneration),
        HashCombine(GetTypeHash(Value.PlayerGeneration), GetTypeHash(Value.SpawnGeneration)));
}
