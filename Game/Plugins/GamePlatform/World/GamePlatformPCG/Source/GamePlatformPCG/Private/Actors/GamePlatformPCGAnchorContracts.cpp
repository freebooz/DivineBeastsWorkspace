#include "Services/GamePlatformPCGAnchorContracts.h"

#include "Misc/SecureHash.h"
#include "Types/GamePlatformPCGDomainIds.h"

bool FGamePlatformPCGAnchorRules::MakeStableId(const FGamePlatformId& WorldId,
    const FGamePlatformId& RegionId, FGuid SourceId, FName DomainId,
    int32 ContentRevision, int32 SlotIndex, FGuid& OutStableId)
{
    OutStableId.Invalidate();
    const bool bValidDomain =
        DomainId == FGamePlatformPCGDomainIds::PlayResource ||
        DomainId == FGamePlatformPCGDomainIds::PlayCover ||
        DomainId == FGamePlatformPCGDomainIds::PlayClimb ||
        DomainId == FGamePlatformPCGDomainIds::PlaySpawn ||
        DomainId == FGamePlatformPCGDomainIds::StateHarvest ||
        DomainId == FGamePlatformPCGDomainIds::StateChop ||
        DomainId == FGamePlatformPCGDomainIds::StateGate ||
        DomainId == FGamePlatformPCGDomainIds::StatePersist;
    if (!WorldId.IsValid() || !RegionId.IsValid() || !SourceId.IsValid() ||
        !bValidDomain || ContentRevision <= 0 || SlotIndex < 0)
    {
        return false;
    }

    // 只使用不随Actor数组顺序变化的语义字段，UTF8规范化防止多端ANSI系统编码差异。
    const FString Identity = FString::Printf(TEXT("PCGAnchorV1|%s|%s|%s|%s|%d|%d"),
        *WorldId.ToString(), *RegionId.ToString(),
        *SourceId.ToString(EGuidFormats::Digits), *DomainId.ToString(),
        ContentRevision, SlotIndex);
    const FTCHARToUTF8 Utf8(*Identity);
    const FString Hash = FMD5::HashBytes(
        reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<uint64>(Utf8.Length()));
    return FGuid::ParseExact(Hash, EGuidFormats::Digits, OutStableId) && OutStableId.IsValid();
}

bool FGamePlatformPCGAnchorRules::ValidateAuthoritativeStates(
    TConstArrayView<FGamePlatformPCGAnchorCandidate> Anchors,
    TConstArrayView<FGamePlatformPCGObjectStateSnapshot> Snapshots)
{
    if (Anchors.Num() > 4096 || Snapshots.Num() > Anchors.Num())
    {
        return false;
    }
    TMap<FGuid, int32> Known;
    for (const FGamePlatformPCGAnchorCandidate& Anchor : Anchors)
    {
        if (!Anchor.StableId.IsValid() || !Anchor.SourceId.IsValid() ||
            Anchor.ContentRevision <= 0 || Anchor.WorldTransform.ContainsNaN() ||
            Known.Contains(Anchor.StableId))
        {
            return false;
        }
        Known.Add(Anchor.StableId, Anchor.ContentRevision);
    }

    TSet<FGuid> Seen;
    for (const FGamePlatformPCGObjectStateSnapshot& State : Snapshots)
    {
        const int32* Revision = Known.Find(State.StableId);
        if (!Revision || *Revision != State.ContentRevision ||
            State.ServerSequence <= 0 || State.StateCode < 0 ||
            Seen.Contains(State.StableId))
        {
            // 未知锚点、版本变化、重复更新或非权威序列号：安全拒绝映射。
            return false;
        }
        Seen.Add(State.StableId);
    }
    return true;
}
