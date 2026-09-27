#include "Services/GamePlatformQuestClientSubsystem.h"

#include "Settings/GamePlatformQuestSettings.h"

void UGamePlatformQuestClientSubsystem::Deinitialize()
{
    QuestCache.Reset();
    TrackedQuestIds.Reset();
    CachedSortedSnapshots.Reset();
    bSortedSnapshotsDirty = true;
    OnChanged.Clear();
    Super::Deinitialize();
}

bool UGamePlatformQuestClientSubsystem::ApplyAuthoritativeSnapshots(
    const TArray<FGamePlatformQuestSnapshot>& Snapshots)
{
    bool bChanged = false;

    for (const FGamePlatformQuestSnapshot& Snapshot : Snapshots)
    {
        if (Snapshot.QuestId.IsNone())
        {
            continue;
        }

        FGamePlatformQuestSnapshot* Existing =
            QuestCache.Find(Snapshot.QuestId);

        if (Existing && Snapshot.Revision < Existing->Revision)
        {
            continue;
        }

        if (!Existing ||
            Snapshot.Revision > Existing->Revision ||
            Snapshot.State != Existing->State)
        {
            QuestCache.Add(Snapshot.QuestId, Snapshot);
            bChanged = true;
        }
    }

    if (bChanged)
    {
        bSortedSnapshotsDirty = true;
        OnChanged.Broadcast();
    }

    return bChanged;
}

bool UGamePlatformQuestClientSubsystem::TrackQuest(FName QuestId)
{
    if (QuestId.IsNone() || !QuestCache.Contains(QuestId))
    {
        return false;
    }

    const int32 MaxTracked =
        FMath::Max(
            1,
            GetDefault<UGamePlatformQuestSettings>()
                ->ClientMaxTrackedQuests);

    if (!TrackedQuestIds.Contains(QuestId) &&
        TrackedQuestIds.Num() >= MaxTracked)
    {
        return false;
    }

    const bool bWasTracked = TrackedQuestIds.Contains(QuestId);
    TrackedQuestIds.Add(QuestId);

    if (!bWasTracked)
    {
        OnChanged.Broadcast();
    }

    return !bWasTracked;
}

bool UGamePlatformQuestClientSubsystem::UntrackQuest(FName QuestId)
{
    const int32 Removed = TrackedQuestIds.Remove(QuestId);
    if (Removed > 0)
    {
        OnChanged.Broadcast();
        return true;
    }
    return false;
}

bool UGamePlatformQuestClientSubsystem::IsTracked(FName QuestId) const
{
    return TrackedQuestIds.Contains(QuestId);
}

const FGamePlatformQuestSnapshot*
UGamePlatformQuestClientSubsystem::FindQuest(FName QuestId) const
{
    return QuestCache.Find(QuestId);
}

void UGamePlatformQuestClientSubsystem::EnsureSortedSnapshotsCache() const
{
    if (!bSortedSnapshotsDirty)
    {
        return;
    }

    CachedSortedSnapshots.Reset();
    CachedSortedSnapshots.Reserve(QuestCache.Num());
    QuestCache.GenerateValueArray(CachedSortedSnapshots);
    CachedSortedSnapshots.Sort(
        [](const FGamePlatformQuestSnapshot& A,
           const FGamePlatformQuestSnapshot& B)
        {
            if (A.State != B.State)
            {
                return static_cast<uint8>(A.State) <
                       static_cast<uint8>(B.State);
            }
            return A.QuestId.LexicalLess(B.QuestId);
        });
    bSortedSnapshotsDirty = false;
}

const TArray<FGamePlatformQuestSnapshot>&
UGamePlatformQuestClientSubsystem::GetSortedSnapshotsView() const
{
    EnsureSortedSnapshotsCache();
    return CachedSortedSnapshots;
}

TArray<FGamePlatformQuestSnapshot>
UGamePlatformQuestClientSubsystem::GetSortedSnapshots() const
{
    return GetSortedSnapshotsView();
}
