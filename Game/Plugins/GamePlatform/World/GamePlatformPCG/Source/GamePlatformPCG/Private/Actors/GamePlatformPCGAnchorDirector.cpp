#include "Actors/GamePlatformPCGActors.h"

#include "Services/GamePlatformPCGAnchorContracts.h"
#include "Types/GamePlatformPCGDomainIds.h"

/** 从已注册的原语放置器生成稳定Gameplay候选，不调用玩法、背包、存档或NavMesh。 */
bool AGamePlatformPCGWorldDirector::CollectGameplayAnchorCandidates(
    const FGamePlatformId& WorldId, const FGamePlatformId& RegionId,
    int32 ContentRevision, TArray<FGamePlatformPCGAnchorCandidate>& OutCandidates,
    FString& OutError) const
{
    check(IsInGameThread());
    OutCandidates.Reset();
    if (!ValidateParticipantSet(OutError) ||
        !WorldId.IsValid() || !RegionId.IsValid() || ContentRevision <= 0)
    {
        OutError = TEXT("Gameplay候选需要有效的世界/区域/内容修订及合法注册集合。");
        return false;
    }

    const TArray<AGamePlatformPCGActorBase*> Anchors =
        GetParticipantsForStage(EGamePlatformPCGWorldStage::GameplayAnchors);
    if (Anchors.Num() > 4096)
    {
        OutError = TEXT("GameplayAnchors超过4096对象预算。");
        return false;
    }

    TSet<FGuid> UniqueIds;
    for (AGamePlatformPCGActorBase* Actor : Anchors)
    {
        const bool bSupported =
            IsValid(Actor) &&
            (Actor->DomainId == FGamePlatformPCGDomainIds::PlayResource ||
             Actor->DomainId == FGamePlatformPCGDomainIds::PlayCover ||
             Actor->DomainId == FGamePlatformPCGDomainIds::PlayClimb ||
             Actor->DomainId == FGamePlatformPCGDomainIds::PlaySpawn);
        if (!bSupported || (Actor->OwnerRegionId.IsValid() && Actor->OwnerRegionId != RegionId) ||
            Actor->GetActorTransform().ContainsNaN())
        {
            OutCandidates.Reset();
            OutError = TEXT("Gameplay锚点必须指定已批准Play.*领域、同区域身份及有限位置。");
            return false;
        }

        FGamePlatformPCGAnchorCandidate Candidate;
        Candidate.SourceId = Actor->SourceId;
        Candidate.DomainId = Actor->DomainId;
        Candidate.ContentRevision = ContentRevision;
        Candidate.WorldTransform = Actor->GetActorTransform();
        if (!FGamePlatformPCGAnchorRules::MakeStableId(WorldId, RegionId, Candidate.SourceId,
                Candidate.DomainId, ContentRevision, 0, Candidate.StableId) ||
            UniqueIds.Contains(Candidate.StableId))
        {
            OutCandidates.Reset();
            OutError = TEXT("Gameplay锚点稳定ID无法创建或存在重复，禁止发布可能影响存档的布局。");
            return false;
        }
        UniqueIds.Add(Candidate.StableId);
        OutCandidates.Add(MoveTemp(Candidate));
    }
    OutError.Reset();
    return true;
}
