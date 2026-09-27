#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformQuestTypes.h"
#include "GamePlatformQuestClientSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FGamePlatformQuestClientChanged);

UCLASS()
class GAMEPLATFORMQUESTCLIENT_API UGamePlatformQuestClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    bool ApplyAuthoritativeSnapshots(
        const TArray<FGamePlatformQuestSnapshot>& Snapshots);

    bool TrackQuest(FName QuestId);
    bool UntrackQuest(FName QuestId);
    bool IsTracked(FName QuestId) const;

    const FGamePlatformQuestSnapshot* FindQuest(FName QuestId) const;
    TArray<FGamePlatformQuestSnapshot> GetSortedSnapshots() const;

    /** C++高频UI读取使用：仅在权威Quest Cache变化后重建排序缓存。 */
    const TArray<FGamePlatformQuestSnapshot>& GetSortedSnapshotsView() const;
    const TSet<FName>& GetTrackedQuestIds() const { return TrackedQuestIds; }

    FGamePlatformQuestClientChanged OnChanged;

private:
    TMap<FName, FGamePlatformQuestSnapshot> QuestCache;
    TSet<FName> TrackedQuestIds;

    mutable bool bSortedSnapshotsDirty = true;
    mutable TArray<FGamePlatformQuestSnapshot> CachedSortedSnapshots;
    void EnsureSortedSnapshotsCache() const;
};
