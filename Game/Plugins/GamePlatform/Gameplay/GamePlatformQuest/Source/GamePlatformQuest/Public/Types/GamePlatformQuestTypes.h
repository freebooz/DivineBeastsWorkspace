#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformQuestState : uint8
{
    Locked,
    Available,
    Accepted,
    Active,
    ObjectivesCompleted,
    CompletionPending,
    Completed,
    Abandoned,
    Failed,
    Expired
};

UENUM(BlueprintType)
enum class EGamePlatformQuestObjectiveType : uint8
{
    Interaction,
    Combat,
    Region,
    Tutorial
};

UENUM(BlueprintType)
enum class EGamePlatformQuestAggregationPolicy : uint8
{
    Count,
    Sum,
    SetComplete
};

UENUM(BlueprintType)
enum class EGamePlatformQuestRepeatPolicy : uint8
{
    OneShot,
    RepeatableManual,
    PeriodicUnsupported
};

UENUM(BlueprintType)
enum class EGamePlatformQuestTimeWindowPolicy : uint8
{
    None,
    PeriodicUnsupported
};

UENUM(BlueprintType)
enum class EGamePlatformQuestRewardStatus : uint8
{
    NoReward,
    PendingExternalReward,
    UnsupportedRewardType,
    Granted,
    Failed
};

UENUM(BlueprintType)
enum class EGamePlatformQuestError : uint8
{
    None,
    QuestNotFound,
    DefinitionMissing,
    DefinitionVersionMismatch,
    QuestLocked,
    QuestNotAvailable,
    AlreadyAccepted,
    AlreadyCompleted,
    QuestExpired,
    PrerequisiteNotMet,
    ObjectiveNotFound,
    InvalidQuestEvent,
    DuplicateEvent,
    RevisionConflict,
    PersistenceUnavailable,
    PersistenceOutcomeUnknown,
    CompletionAlreadyCommitted,
    RewardUnsupported,
    RewardPending,
    RewardFailed,
    Unauthorized,
    ServerNotAuthoritative,
    Unsupported,
    Cancelled,
    TimedOut
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMQUEST_API FGamePlatformQuestObjectiveProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FName ObjectiveId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    double CurrentValue = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    double RequiredValue = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMQUEST_API FGamePlatformQuestSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FName QuestId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FGuid QuestInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    int32 DefinitionVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestState State = EGamePlatformQuestState::Locked;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    TArray<FGamePlatformQuestObjectiveProgress> Objectives;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FGuid CompletionId;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FName RewardSetId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FGuid RewardClaimId;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestRewardStatus RewardStatus =
        EGamePlatformQuestRewardStatus::NoReward;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FString PeriodId;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FDateTime AcceptedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FDateTime CompletedAtUtc;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMQUEST_API FGamePlatformQuestEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGuid EventId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FName EventType = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FString PlayerId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGuid PlayerRuntimeId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGuid CharacterId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FName WorldId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FName RegionId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGuid SourceEntityId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGuid TargetEntityId;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FGameplayTagContainer SemanticTags;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    double NumericValue = 1.0;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    FDateTime OccurredAtUtc;

    UPROPERTY(BlueprintReadWrite, Category="Quest")
    int32 SourceGeneration = 0;
};
