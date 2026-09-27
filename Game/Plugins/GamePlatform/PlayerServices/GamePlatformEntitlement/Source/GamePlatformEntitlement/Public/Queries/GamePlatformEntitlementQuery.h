#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEntitlementTypes.h"

class GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementQuery
{
public:
    static bool HasEntitlement(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        FName EntitlementId);

    static bool HasAny(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        const TArray<FName>& EntitlementIds);

    static bool HasAll(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        const TArray<FName>& EntitlementIds);

    static bool HasTarget(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        EGamePlatformEntitlementTargetType TargetType,
        FName TargetId);

    static const FGamePlatformEntitlementEntry* FindEntitlement(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        FName EntitlementId);
};
