#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationContext.generated.h"

/** EGamePlatformPresentationLocalPlayerRelation（表现目标与本地玩家关系）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationLocalPlayerRelation : uint8
{
    Unknown,
    Self,
    Ally,
    Enemy,
    Neutral
};

/** EGamePlatformPresentationQualityTier（表现质量档位提示）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationQualityTier : uint8
{
    Unknown,
    Low,
    Medium,
    High,
    Epic
};

/** EGamePlatformPresentationContextScope（上下文贡献作用域）。 */
UENUM()
enum class EGamePlatformPresentationContextScope : uint8
{
    LocalPlayer,
    World,
    Session
};

/** EGamePlatformPresentationConflictPolicy（上下文字段冲突策略）。 */
UENUM()
enum class EGamePlatformPresentationConflictPolicy : uint8
{
    FillMissing,
    Override,
    RejectConflict
};

/** FGamePlatformPresentationContext（平台类型化表现上下文）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProjectId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SkinId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WorldId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ExperienceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArenaModeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ContentPackId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlatformId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationLocalPlayerRelation LocalPlayerRelation =
        EGamePlatformPresentationLocalPlayerRelation::Unknown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationQualityTier QualityTier =
        EGamePlatformPresentationQualityTier::Unknown;
};

/**
 * FGamePlatformPresentationContextPatch（上下文贡献补丁）。
 * NAME_None/0/Unknown表示“不贡献该字段”。
 */
USTRUCT()
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationContextPatch
{
    GENERATED_BODY()

    FGamePlatformPresentationContext Values;
};

/** IGamePlatformPresentationContextContributor（平台表现上下文贡献者）。 */
class GAMEPLATFORMPRESENTATIONCORE_API IGamePlatformPresentationContextContributor
{
public:
    virtual ~IGamePlatformPresentationContextContributor() = default;

    virtual FName GetContributorId() const = 0;
    virtual int32 GetPriority() const = 0;
    virtual EGamePlatformPresentationContextScope GetScope() const = 0;
    virtual EGamePlatformPresentationConflictPolicy GetConflictPolicy() const = 0;
    virtual FGamePlatformPresentationContextPatch BuildPatch() const = 0;
};
