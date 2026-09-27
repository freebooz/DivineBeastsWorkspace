#pragma once

#include "CoreMinimal.h"
#include "GamePlatformInteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformInteractionMode : uint8
{
    Instant,
    Hold
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionConcurrencyPolicy : uint8
{
    Exclusive,
    Shared
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionSessionState : uint8
{
    None,
    Requested,
    Validating,
    Active,
    Committing,
    Completed,
    Rejected,
    Cancelled,
    Failed,
    TimedOut
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionError : uint8
{
    None,
    NoFocusedTarget,
    InvalidInteractor,
    InteractorNotActive,
    InvalidTarget,
    TargetDestroyed,
    TargetUnavailable,
    InvalidOption,
    OutOfRange,
    LineOfSightBlocked,
    TargetBusy,
    ConcurrentLimitReached,
    DuplicateRequest,
    StaleTargetGeneration,
    StaleTargetRevision,
    SessionNotFound,
    SessionAlreadyCompleted,
    UserCancelled,
    TimedOut,
    RateLimited,
    WorldTearingDown,
    OutcomeHandlerUnavailable,
    OutcomeUnknown,
    Unsupported
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionCancelReason : uint8
{
    None,
    UserCancelled,
    OutOfRange,
    LineOfSightLost,
    TargetUnavailable,
    TargetDestroyed,
    InteractorInvalid,
    InteractorNotActive,
    TargetRevisionChanged,
    WorldTearingDown,
    TimedOut,
    ReplacedByNewSession
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionCommitKind : uint8
{
    Custom,
    Toggle,
    Consume,
    Harvest,
    /** 提交成功仅代表进入外部权威处理，不直接修改目标最终状态。 */
    External
};

UENUM(BlueprintType)
enum class EGamePlatformInteractionEventType : uint8
{
    FocusChanged,
    InteractionStarted,
    InteractionCancelled,
    InteractionCommitted,
    PickupConsumed,
    HarvestCompleted
};
