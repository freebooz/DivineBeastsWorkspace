#pragma once

#include "CoreMinimal.h"
#include "GamePlatformEntitlementTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEntitlementTargetType : uint8
{
    Unknown,
    Hero,
    Skin,
    Cosmetic,
    Feature
};

UENUM(BlueprintType)
enum class EGamePlatformEntitlementStatus : uint8
{
    Active,
    NotStarted,
    Expired,
    NotEntitled,
    Revoked
};

UENUM(BlueprintType)
enum class EGamePlatformEntitlementError : uint8
{
    None,
    EntitlementNotFound,
    DefinitionNotFound,
    NotEntitled,
    EntitlementExpired,
    EntitlementNotStarted,
    SnapshotUnavailable,
    RevisionConflict,
    DuplicateOperation,
    GrantNotAuthorized,
    RevokeNotAuthorized,
    GrantNotFound,
    InvalidValidityWindow,
    InvalidSource,
    OutcomeUnknown,
    BackendUnavailable,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName Category = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementTargetType TargetType =
        EGamePlatformEntitlementTargetType::Unknown;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName TargetId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementStatus Status =
        EGamePlatformEntitlementStatus::Active;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasStartsAt = false;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime StartsAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasExpiresAt = false;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime ExpiresAtUtc;

    bool IsEffective() const
    {
        return !EntitlementId.IsNone() &&
               Status == EGamePlatformEntitlementStatus::Active;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    TArray<FGamePlatformEntitlementEntry> Entitlements;

    bool IsValid() const
    {
        return Revision > 0 && GeneratedAtUtc.GetTicks() > 0;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementCheckResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bEntitled = false;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementStatus Status =
        EGamePlatformEntitlementStatus::Expired;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;
};
