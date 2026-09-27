#pragma once

#include "CoreMinimal.h"
#include "GamePlatformProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformProgressionSubjectType : uint8
{
    Unknown,
    Character,
    Player
};

UENUM(BlueprintType)
enum class EGamePlatformProgressionPostMaxXPPolicy : uint8
{
    ClampAtMax
};

UENUM(BlueprintType)
enum class EGamePlatformProgressionError : uint8
{
    None,
    ProgressionNotFound,
    TrackNotFound,
    CurveNotFound,
    CurveVersionMismatch,
    InvalidCurve,
    InvalidXPAmount,
    XPOverflow,
    MaxLevelReached,
    RevisionConflict,
    DuplicateOperation,
    OperationInProgress,
    GrantNotAuthorized,
    InvalidSource,
    RequirementNotMet,
    SnapshotUnavailable,
    BackendUnavailable,
    OutcomeUnknown,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionTrackState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Unknown;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FString SubjectId;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 TotalXP = 0;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 MaxLevel = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurveVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 ProgressionRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FDateTime GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    TArray<FGamePlatformProgressionTrackState> Tracks;

    bool IsValid() const
    {
        return ProgressionRevision > 0 &&
               GeneratedAtUtc.GetTicks() > 0;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionRequirement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Character;

    UPROPERTY(BlueprintReadWrite, Category="Progression")
    FString SubjectId;

    UPROPERTY(BlueprintReadWrite, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Progression", meta=(ClampMin="1"))
    int32 RequiredLevel = 1;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionRequirementResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    bool bMet = false;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurrentLevel = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 Revision = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurveVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;
};
