#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "GamePlatformEntitlementClientTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEntitlementClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENTCLIENT_API FGamePlatformEntitlementViewModel
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
    bool bUnlocked = false;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasExpiresAt = false;

    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime ExpiresAtUtc;
};
