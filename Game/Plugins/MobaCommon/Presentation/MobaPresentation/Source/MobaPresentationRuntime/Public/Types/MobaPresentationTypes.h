#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformPresentationTypes.h"
#include "MobaPresentationTypes.generated.h"

/** EMobaPresentationAbilityFactType（MOBA技能表现事实类型）。 */
UENUM(BlueprintType)
enum class EMobaPresentationAbilityFactType : uint8
{
    CastStart,
    CastRelease,
    ProjectileSpawn,
    AreaWarning
};

/** EMobaPresentationStatusFactType（MOBA状态表现事实类型）。 */
UENUM(BlueprintType)
enum class EMobaPresentationStatusFactType : uint8
{
    Apply,
    Remove
};

/** EMobaPresentationCharacterFactType（MOBA角色生命周期表现事实类型）。 */
UENUM(BlueprintType)
enum class EMobaPresentationCharacterFactType : uint8
{
    Death,
    Respawn
};

/** EMobaPresentationArenaFactType（MOBA竞技表现事实类型）。 */
UENUM(BlueprintType)
enum class EMobaPresentationArenaFactType : uint8
{
    MatchStart,
    MatchEnd,
    ScoreChanged,
    ObjectiveCompleted
};

/** FMobaPresentationFactIdentity（MOBA表现事实身份）。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationFactIdentity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FactId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPredicted = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bConfirmed = true;

    bool IsValid() const { return FactId.IsValid() && Revision >= 0; }
};

/** FMobaPresentationContext（MOBA表现上下文）。稳定字段使用明确类型，不使用字符串Map承载核心语义。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MatchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArenaModeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TeamId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString HeroDefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StatusId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector SourceLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCritical = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPredicted = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bConfirmed = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLocalSource = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLocalTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequestGeneration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer AdditionalTags;

    bool IsStructurallyValid() const
    {
        return WorldGeneration >= 0 && AvatarGeneration >= 0 && RequestGeneration >= 0;
    }
};

/** Hit（命中）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationHitPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactNormal = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCritical = false;
};

/** Critical（暴击）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationCriticalPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.0f;
};

/** Heal（治疗）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationHealPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Amount = 0.0f;
};

/** Shield（护盾命中）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationShieldPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AbsorbedAmount = 0.0f;
};

/** Control（控制）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationControlPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag ControlTag;
};

/** Ability Cast（技能施法）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationAbilityCastPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRelease = false;
};

/** Projectile（投射物表现）载荷；不代表Gameplay Actor创建。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationProjectilePayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector SourceLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetLocation = FVector::ZeroVector;
};

/** Area Warning（范围预警）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationAreaWarningPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 0.0f;
};

/** Status（持续状态）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationStatusPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StatusId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DisplayDurationSeconds = 0.0f;
};

/** Death（死亡）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationDeathPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString KillerEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
};

/** Respawn（复活）表现载荷。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationRespawnPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
};

/** Critical（暴击）确认事实。只有上游明确确认暴击时才能产生，不能由客户端数值差推断。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationCriticalFact
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.0f;
};

/** Ability（技能）公共表现事实；由真实Ability公共通知适配，不创建Gameplay投射物。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationAbilityFact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMobaPresentationAbilityFactType Type = EMobaPresentationAbilityFactType::CastStart;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector SourceLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.0f;
};

/** Status（状态）公共表现事实。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationStatusFact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMobaPresentationStatusFactType Type = EMobaPresentationStatusFactType::Apply;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StatusId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DisplayDurationSeconds = 0.0f;
};

/** Character（角色生命周期）公共表现事实。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationCharacterFact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMobaPresentationCharacterFactType Type = EMobaPresentationCharacterFactType::Death;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RelatedEntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
};

/** Arena（竞技）公共表现事实。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationArenaFact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMobaPresentationArenaFactType Type = EMobaPresentationArenaFactType::MatchStart;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MatchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArenaModeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TeamId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 0;
};

/** 适配完成、等待提交到GamePlatformPresentation（平台表现协调层）的中立事实。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationAdaptedFact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationFactIdentity Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag Semantic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMobaPresentationContext Context;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGamePlatformPresentationPriority Priority = EGamePlatformPresentationPriority::Normal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGamePlatformPresentationLifetime Lifetime = EGamePlatformPresentationLifetime::Instant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTransient = true;

    bool IsValid() const
    {
        return Identity.IsValid() && Semantic.IsValid() && Context.IsStructurallyValid();
    }
};
