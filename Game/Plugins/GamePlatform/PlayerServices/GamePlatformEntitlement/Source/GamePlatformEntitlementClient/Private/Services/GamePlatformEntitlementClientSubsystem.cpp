#include "Services/GamePlatformEntitlementClientSubsystem.h"

#include "Interfaces/GamePlatformEntitlementClientTransport.h"
#include "Queries/GamePlatformEntitlementQuery.h"

void UGamePlatformEntitlementClientSubsystem::Deinitialize()
{
    OnChanged.Clear();
    ResetAccount();
    Super::Deinitialize();
}

bool UGamePlatformEntitlementClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe> InTransport)
{
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    return RefreshSnapshot();
}

void UGamePlatformEntitlementClientSubsystem::ResetAccount()
{
    ++AccountGeneration;

    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Transport.Reset();
    Snapshot = {};
    EffectiveEntitlementIds.Reset();
    EffectiveHeroIds.Reset();
    EffectiveSkinIds.Reset();
    CachedViewModels.Reset();
    LastError = EGamePlatformEntitlementError::None;
    State = EGamePlatformEntitlementClientState::Uninitialized;
    OnChanged.Broadcast();
}

bool UGamePlatformEntitlementClientSubsystem::RefreshSnapshot()
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        State == EGamePlatformEntitlementClientState::Loading ||
        State == EGamePlatformEntitlementClientState::Reconciling)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    State =
        Snapshot.Revision > 0
            ? EGamePlatformEntitlementClientState::Reconciling
            : EGamePlatformEntitlementClientState::Loading;
    LastError = EGamePlatformEntitlementError::None;
    OnChanged.Broadcast();

    TWeakObjectPtr<UGamePlatformEntitlementClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetSnapshot(
            [WeakThis, ExpectedGeneration](
                FGamePlatformEntitlementSnapshot NewSnapshot,
                EGamePlatformEntitlementError Error)
            {
                if (UGamePlatformEntitlementClientSubsystem* Self =
                    WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted)
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = EGamePlatformEntitlementError::BackendUnavailable;
        OnChanged.Broadcast();
    }

    return bStarted;
}

bool UGamePlatformEntitlementClientSubsystem::HasEntitlement(
    FName EntitlementId) const
{
    return !EntitlementId.IsNone() && EffectiveEntitlementIds.Contains(EntitlementId);
}

bool UGamePlatformEntitlementClientSubsystem::HasAny(
    const TArray<FName>& EntitlementIds) const
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (EffectiveEntitlementIds.Contains(EntitlementId))
        {
            return true;
        }
    }
    return false;
}

bool UGamePlatformEntitlementClientSubsystem::HasAll(
    const TArray<FName>& EntitlementIds) const
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (!EffectiveEntitlementIds.Contains(EntitlementId))
        {
            return false;
        }
    }
    return true;
}

TArray<FGamePlatformEntitlementViewModel>
UGamePlatformEntitlementClientSubsystem::GetViewModels() const
{
    return CachedViewModels;
}

bool UGamePlatformEntitlementClientSubsystem::IsHeroUnlocked(
    FName HeroDefinitionId) const
{
    return !HeroDefinitionId.IsNone() && EffectiveHeroIds.Contains(HeroDefinitionId);
}

bool UGamePlatformEntitlementClientSubsystem::IsSkinUnlocked(
    FName SkinDefinitionId) const
{
    return !SkinDefinitionId.IsNone() && EffectiveSkinIds.Contains(SkinDefinitionId);
}

void UGamePlatformEntitlementClientSubsystem::RebuildDerivedCaches()
{
    EffectiveEntitlementIds.Reset();
    EffectiveHeroIds.Reset();
    EffectiveSkinIds.Reset();
    CachedViewModels.Reset();

    EffectiveEntitlementIds.Reserve(Snapshot.Entitlements.Num());
    EffectiveHeroIds.Reserve(Snapshot.Entitlements.Num());
    EffectiveSkinIds.Reserve(Snapshot.Entitlements.Num());
    CachedViewModels.Reserve(Snapshot.Entitlements.Num());

    for (const FGamePlatformEntitlementEntry& Entry : Snapshot.Entitlements)
    {
        if (Entry.IsEffective())
        {
            EffectiveEntitlementIds.Add(Entry.EntitlementId);
            if (Entry.TargetType == EGamePlatformEntitlementTargetType::Hero && !Entry.TargetId.IsNone())
            {
                EffectiveHeroIds.Add(Entry.TargetId);
            }
            else if (Entry.TargetType == EGamePlatformEntitlementTargetType::Skin && !Entry.TargetId.IsNone())
            {
                EffectiveSkinIds.Add(Entry.TargetId);
            }
        }

        FGamePlatformEntitlementViewModel View;
        View.EntitlementId = Entry.EntitlementId;
        View.Category = Entry.Category;
        View.TargetType = Entry.TargetType;
        View.TargetId = Entry.TargetId;
        View.bUnlocked = Entry.IsEffective();
        View.bHasExpiresAt = Entry.bHasExpiresAt;
        View.ExpiresAtUtc = Entry.ExpiresAtUtc;
        CachedViewModels.Add(MoveTemp(View));
    }
}

void UGamePlatformEntitlementClientSubsystem::HandleSnapshotCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformEntitlementSnapshot NewSnapshot,
    EGamePlatformEntitlementError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    if (Error != EGamePlatformEntitlementError::None)
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = Error;
        OnChanged.Broadcast();
        return;
    }

    if (!ApplySnapshot(NewSnapshot))
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = EGamePlatformEntitlementError::InvalidResponse;
        OnChanged.Broadcast();
        return;
    }

    State = EGamePlatformEntitlementClientState::Ready;
    LastError = EGamePlatformEntitlementError::None;
    OnChanged.Broadcast();
}

bool UGamePlatformEntitlementClientSubsystem::ApplySnapshot(
    const FGamePlatformEntitlementSnapshot& NewSnapshot)
{
    if (!NewSnapshot.IsValid())
    {
        return false;
    }

    if (Snapshot.Revision > 0 &&
        NewSnapshot.Revision < Snapshot.Revision)
    {
        return false;
    }

    Snapshot = NewSnapshot;
    RebuildDerivedCaches();
    return true;
}
