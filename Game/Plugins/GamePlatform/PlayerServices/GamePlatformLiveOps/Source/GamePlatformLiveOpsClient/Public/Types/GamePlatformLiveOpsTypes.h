#pragma once

#include "CoreMinimal.h"
#include "GamePlatformLiveOpsTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformLiveOpsError : uint8
{
    None,
    CatalogUnavailable,
    CatalogRevisionMismatch,
    CampaignNotFound,
    CampaignNotActive,
    NotEligible,
    AlreadyClaimed,
    ClaimInProgress,
    DuplicateOperation,
    InvalidPeriod,
    RewardPreflightFailed,
    RewardGrantFailed,
    OutcomeUnknown,
    BackendUnavailable,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
};

UENUM(BlueprintType)
enum class EGamePlatformLiveOpsClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsTimeWindow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime StartsAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bHasEnd = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime EndsAtUtc;

    bool IsActive(const FDateTime& NowUtc) const
    {
        return NowUtc >= StartsAtUtc &&
               (!bHasEnd || NowUtc < EndsAtUtc);
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSeason
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName SeasonId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Version = 1;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString NameKey;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventType = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName SeasonId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Priority = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSignInCampaign
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Version = 1;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 RewardCount = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsCatalogSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 CatalogVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 CatalogRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime PublishedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsSeason> Seasons;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsEvent> Events;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsSignInCampaign> SignInCampaigns;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsCampaignState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString CurrentPeriodKey;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimedCurrentPeriod = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 TotalClaimCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 NextRewardIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString RewardStatus;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsPlayerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 PlayerStateRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsCampaignState> CampaignStates;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsClaimResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString ClaimId;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString ClaimOperationId;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString PeriodKey;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString Status;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 PlayerStateRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsEventViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilStartSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilEndSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSignInViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimable = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimedCurrentPeriod = false;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 TotalClaimCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 NextRewardIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilStartSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilEndSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString RewardStatus;
};
