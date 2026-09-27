#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformAITypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformAIBrainType : uint8
{
    None,
    BehaviorTree,
    StateTree
};

UENUM(BlueprintType)
enum class EGamePlatformAIPublicState : uint8
{
    Idle,
    Moving,
    Investigating,
    Chasing,
    Attacking,
    Controlled,
    Dead,
    Disabled
};

UENUM(BlueprintType)
enum class EGamePlatformAIScalabilityTier : uint8
{
    Critical,
    Active,
    Background,
    Dormant
};

UENUM(BlueprintType)
enum class EGamePlatformAIError : uint8
{
    None,
    InvalidDefinition,
    MissingStateComponent,
    UnsupportedBrain,
    AssetLoadFailed,
    InvalidPawn,
    NotAuthority,
    NavigationUnavailable,
    NoValidTarget,
    AbilityUnavailable,
    WorldTearingDown
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMAI_API FGamePlatformAIPerceptionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception", meta=(ClampMin="0.0"))
    float SightRadius = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception", meta=(ClampMin="0.0"))
    float LoseSightRadius = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception", meta=(ClampMin="0.0", ClampMax="180.0"))
    float PeripheralVisionHalfAngleDegrees = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception", meta=(ClampMin="0.0"))
    float MaxAge = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception")
    bool bEnableHearing = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception", meta=(ClampMin="0.0"))
    float HearingRange = 1600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Perception")
    bool bEnableDamageSense = true;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMAI_API FGamePlatformAITargetSelectionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Targeting", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxCandidates = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Targeting", meta=(ClampMin="0.1"))
    float MemorySeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Targeting")
    bool bPreferVisibleTargets = true;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMAI_API FGamePlatformAIHomePolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Home", meta=(ClampMin="0.0"))
    float PatrolRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Home", meta=(ClampMin="0.0"))
    float LeashRadius = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Home", meta=(ClampMin="0.0"))
    float ReturnHomeAcceptanceRadius = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Home", meta=(ClampMin="0.0"))
    float PatrolAcceptanceRadius = 80.0f;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMAI_API FGamePlatformAIUpdateProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Update", meta=(ClampMin="0.05"))
    float DecisionInterval = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Update", meta=(ClampMin="0.05"))
    float MoveRefreshInterval = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Update", meta=(ClampMin="0.0"))
    float MoveRefreshDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Update", meta=(ClampMin="0.05"))
    float AttackRetryBackoff = 0.50f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Update")
    EGamePlatformAIScalabilityTier DefaultTier =
        EGamePlatformAIScalabilityTier::Active;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMAI_API FGamePlatformAIStateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="AI")
    FGuid AIEntityId;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    FName AIDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    EGamePlatformAIPublicState PublicState =
        EGamePlatformAIPublicState::Disabled;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    FGuid CurrentTargetEntityId;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    FGameplayTag MovementIntentTag;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    FGameplayTag CombatIntentTag;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    int32 StateRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    int32 AIInstanceGeneration = 1;
};
