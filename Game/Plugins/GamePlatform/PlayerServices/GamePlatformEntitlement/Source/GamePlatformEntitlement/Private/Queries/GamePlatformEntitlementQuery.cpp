#include "Queries/GamePlatformEntitlementQuery.h"

const FGamePlatformEntitlementEntry*
FGamePlatformEntitlementQuery::FindEntitlement(
    const FGamePlatformEntitlementSnapshot& Snapshot,
    FName EntitlementId)
{
    if (EntitlementId.IsNone())
    {
        return nullptr;
    }

    return Snapshot.Entitlements.FindByPredicate(
        [EntitlementId](const FGamePlatformEntitlementEntry& Entry)
        {
            return Entry.EntitlementId == EntitlementId &&
                   Entry.IsEffective();
        });
}

bool FGamePlatformEntitlementQuery::HasEntitlement(
    const FGamePlatformEntitlementSnapshot& Snapshot,
    FName EntitlementId)
{
    return FindEntitlement(Snapshot, EntitlementId) != nullptr;
}

bool FGamePlatformEntitlementQuery::HasAny(
    const FGamePlatformEntitlementSnapshot& Snapshot,
    const TArray<FName>& EntitlementIds)
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (HasEntitlement(Snapshot, EntitlementId))
        {
            return true;
        }
    }

    return false;
}

bool FGamePlatformEntitlementQuery::HasAll(
    const FGamePlatformEntitlementSnapshot& Snapshot,
    const TArray<FName>& EntitlementIds)
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (!HasEntitlement(Snapshot, EntitlementId))
        {
            return false;
        }
    }

    return true;
}

bool FGamePlatformEntitlementQuery::HasTarget(
    const FGamePlatformEntitlementSnapshot& Snapshot,
    EGamePlatformEntitlementTargetType TargetType,
    FName TargetId)
{
    if (TargetType == EGamePlatformEntitlementTargetType::Unknown ||
        TargetId.IsNone())
    {
        return false;
    }

    return Snapshot.Entitlements.ContainsByPredicate(
        [TargetType, TargetId](const FGamePlatformEntitlementEntry& Entry)
        {
            return Entry.TargetType == TargetType &&
                   Entry.TargetId == TargetId &&
                   Entry.IsEffective();
        });
}
