#pragma once

#include "CoreMinimal.h"
#include "GamePlatformNavigationTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EGamePlatformNavigationError : uint8
{
    None,
    NavigationUnavailable,
    NavDataMissing,
    InvalidWorld,
    StaleWorldGeneration,
    InvalidAgentProfile,
    UnsupportedAgent,
    InvalidStart,
    InvalidGoal,
    ProjectionFailed,
    PathNotFound,
    PartialPathRejected,
    InvalidFilter,
    RequestCancelled,
    RequestTimedOut,
    OwnerDestroyed,
    WorldTearingDown,
    SmartLinkUnavailable,
    TraversalFailed,
    InvokerRegistrationFailed,
    Unsupported
};

UENUM(BlueprintType)
enum class EGamePlatformNavigationPathStatus : uint8
{
    None,
    Success,
    Partial,
    Failed,
    Cancelled,
    TimedOut
};

UENUM(BlueprintType)
enum class EGamePlatformNavigationPartialPathPolicy : uint8
{
    RejectPartial,
    AcceptPartial,
    AcceptForPreviewOnly
};

UENUM(BlueprintType)
enum class EGamePlatformNavigationInvokerPolicy : uint8
{
    Disabled,
    RegisterWhenActive
};

UENUM(BlueprintType)
enum class EGamePlatformNavigationAuthority : uint8
{
    Advisory,
    ServerAuthoritative
};

UENUM(BlueprintType)
enum class EGamePlatformNavigationAreaKind : uint8
{
    Default,
    HighCost,
    Blocked
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationTraversalRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FName NavigationLinkId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FName TraversalType = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    TObjectPtr<AActor> Agent = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FVector Destination = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationPathPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationRequestHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int32 WorldGeneration = 0;

    /** 世界内操作代次，由FindPathAsync返回；0为旧句柄/未接纳请求，取消时拒绝。正int64兼容蓝图读取。 */
    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int64 OperationGeneration = 0;

    bool IsValid() const { return RequestId.IsValid() && WorldGeneration > 0 && OperationGeneration > 0; }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    FGuid RequestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    int32 WorldGeneration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    FName AgentProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    FVector Start = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    FVector Goal = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    FName FilterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    EGamePlatformNavigationPartialPathPolicy PartialPathPolicy =
        EGamePlatformNavigationPartialPathPolicy::RejectPartial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    float TimeoutSeconds = 0.0f;

    /** 可选请求拥有者；异步完成前销毁则丢弃结果。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    TObjectPtr<UObject> OwnerScope = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    EGamePlatformNavigationAuthority Authority =
        EGamePlatformNavigationAuthority::ServerAuthoritative;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationPathResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    EGamePlatformNavigationPathStatus Status =
        EGamePlatformNavigationPathStatus::None;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    EGamePlatformNavigationError Error =
        EGamePlatformNavigationError::None;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int32 WorldGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    TArray<FGamePlatformNavigationPathPoint> PathPoints;

    /** UE厘米。 */
    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    float PathLength = 0.0f;

    /** Navigation System成本；不等于距离。 */
    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    float PathCost = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    bool bIsPartial = false;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    bool bReachedGoal = false;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FVector ResolvedStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FVector ResolvedGoal = FVector::ZeroVector;

    bool IsSuccess() const
    {
        return Status == EGamePlatformNavigationPathStatus::Success ||
               Status == EGamePlatformNavigationPathStatus::Partial;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationProjectionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    FVector ProjectedLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    EGamePlatformNavigationError Error =
        EGamePlatformNavigationError::None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMNAVIGATION_API FGamePlatformNavigationDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int32 WorldGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int32 OutstandingAsyncRequests = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int32 RegisteredInvokers = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int64 SyncQueryCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int64 AsyncQueryCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="Navigation")
    int64 CancelledRequestCount = 0;
};
