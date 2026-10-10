// 本文件属于GamePlatform平台层 GamePlatformPresentation，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "GamePlatformPresentationClientSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Resolution/GamePlatformPresentationCatalogScore.h"
#include "Engine/World.h"

namespace
{
    constexpr int32 MaxPresentationProviders = 32;
    constexpr int32 MaxPresentationContextContributors = 32;
    constexpr int32 MaxPresentationCatalogFragments = 64;
    bool MergeName(
        FName& Target,
        FName Patch,
        EGamePlatformPresentationConflictPolicy Policy)
    {
        if (Patch.IsNone())
        {
            return true;
        }
        if (Target.IsNone())
        {
            Target = Patch;
            return true;
        }
        if (Target == Patch)
        {
            return true;
        }
        if (Policy == EGamePlatformPresentationConflictPolicy::Override)
        {
            Target = Patch;
            return true;
        }
        return Policy == EGamePlatformPresentationConflictPolicy::FillMissing;
    }

    bool MergeInt(
        int32& Target,
        int32 Patch,
        EGamePlatformPresentationConflictPolicy Policy)
    {
        if (Patch == 0)
        {
            return true;
        }
        if (Target == 0)
        {
            Target = Patch;
            return true;
        }
        if (Target == Patch)
        {
            return true;
        }
        if (Policy == EGamePlatformPresentationConflictPolicy::Override)
        {
            Target = Patch;
            return true;
        }
        return Policy == EGamePlatformPresentationConflictPolicy::FillMissing;
    }

    template <typename TEnum>
    bool MergeEnum(
        TEnum& Target,
        TEnum Patch,
        TEnum Unknown,
        EGamePlatformPresentationConflictPolicy Policy)
    {
        if (Patch == Unknown)
        {
            return true;
        }
        if (Target == Unknown)
        {
            Target = Patch;
            return true;
        }
        if (Target == Patch)
        {
            return true;
        }
        if (Policy == EGamePlatformPresentationConflictPolicy::Override)
        {
            Target = Patch;
            return true;
        }
        return Policy == EGamePlatformPresentationConflictPolicy::FillMissing;
    }

    bool ApplyPatch(
        FGamePlatformPresentationContext& Target,
        const FGamePlatformPresentationContextPatch& Patch,
        EGamePlatformPresentationConflictPolicy Policy)
    {
        const FGamePlatformPresentationContext& Value = Patch.Values;
        return MergeName(Target.ProjectId, Value.ProjectId, Policy) &&
               MergeName(Target.HeroDefinitionId, Value.HeroDefinitionId, Policy) &&
               MergeName(Target.AbilityId, Value.AbilityId, Policy) &&
               MergeName(Target.SkinId, Value.SkinId, Policy) &&
               MergeName(Target.WorldId, Value.WorldId, Policy) &&
               MergeName(Target.ExperienceId, Value.ExperienceId, Policy) &&
               MergeName(Target.RegionId, Value.RegionId, Policy) &&
               MergeName(Target.ArenaModeId, Value.ArenaModeId, Policy) &&
               MergeName(Target.ContentPackId, Value.ContentPackId, Policy) &&
               MergeName(Target.PlatformId, Value.PlatformId, Policy) &&
               MergeInt(Target.WorldGeneration, Value.WorldGeneration, Policy) &&
               MergeInt(Target.AvatarGeneration, Value.AvatarGeneration, Policy) &&
               MergeEnum(
                   Target.LocalPlayerRelation,
                   Value.LocalPlayerRelation,
                   EGamePlatformPresentationLocalPlayerRelation::Unknown,
                   Policy) &&
               MergeEnum(
                   Target.QualityTier,
                   Value.QualityTier,
                   EGamePlatformPresentationQualityTier::Unknown,
                   Policy);
    }


}


void UGamePlatformPresentationClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BoundWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    Providers.Reserve(8);
    ContextContributors.Reserve(8);
    CatalogFragments.Reserve(8);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UGamePlatformPresentationClientSubsystem::HandleWorldCleanup);
}

void UGamePlatformPresentationClientSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
    Providers.Reset();
    ContextContributors.Reset();
    CatalogFragments.Reset();
    RequestObserved.Clear();
    BoundWorld.Reset();
    Super::Deinitialize();
}

FGuid UGamePlatformPresentationClientSubsystem::RegisterProvider(
    FName ProviderId,
    int32 Priority,
    FGamePlatformPresentationProviderHandler Handler, TSubclassOf<UObject> DefinitionClass)
{
    if (ProviderId.IsNone() || !Handler.IsBound())
    {
        return FGuid();
    }

    Providers.RemoveAll([ProviderId](const FProviderEntry& Entry)
    {
        return Entry.ProviderId == ProviderId;
    });

    if (Providers.Num() >= MaxPresentationProviders)
    {
        return FGuid();
    }

    FProviderEntry Entry;
    Entry.RegistrationId = FGuid::NewGuid();
    Entry.ProviderId = ProviderId;
    Entry.Priority = Priority;
    Entry.Handler = MoveTemp(Handler);
    Entry.DefinitionClass = DefinitionClass.Get();
    const FGuid Result = Entry.RegistrationId;
    Providers.Add(MoveTemp(Entry));

    Providers.Sort([](const FProviderEntry& A, const FProviderEntry& B)
    {
        if (A.Priority != B.Priority)
        {
            return A.Priority > B.Priority;
        }
        return A.ProviderId.LexicalLess(B.ProviderId);
    });
    return Result;
}

UClass* UGamePlatformPresentationClientSubsystem::GetProviderDefinitionClass(FName ProviderId) const
{
    check(IsInGameThread());
    const auto* Provider = Providers.FindByPredicate([ProviderId](const auto& Item) { return Item.ProviderId == ProviderId; });
    return Provider ? Provider->DefinitionClass.Get() : nullptr;
}

bool UGamePlatformPresentationClientSubsystem::UnregisterProvider(
    const FGuid& RegistrationId)
{
    return RegistrationId.IsValid() &&
        Providers.RemoveAll([&RegistrationId](const FProviderEntry& Entry)
        {
            return Entry.RegistrationId == RegistrationId;
        }) > 0;
}

FGamePlatformPresentationRegistrationHandle
UGamePlatformPresentationClientSubsystem::RegisterContextContributor(
    TSharedRef<IGamePlatformPresentationContextContributor> Contributor)
{
    FGamePlatformPresentationRegistrationHandle Result;
    const FName ContributorId = Contributor->GetContributorId();
    if (ContributorId.IsNone() ||
        ContextContributors.Num() >= MaxPresentationContextContributors ||
        ContextContributors.ContainsByPredicate(
            [ContributorId](const FContextContributorEntry& Existing)
            {
                return Existing.ContributorId == ContributorId;
            }))
    {
        return Result;
    }

    FContextContributorEntry Entry;
    Entry.Handle.Id = FGuid::NewGuid();
    Entry.ContributorId = ContributorId;
    Entry.Priority = Contributor->GetPriority();
    Entry.Scope = Contributor->GetScope();
    Entry.Contributor = Contributor;
    Result = Entry.Handle;
    ContextContributors.Add(MoveTemp(Entry));

    // Contributor注册/注销远低频于Presentation请求；在变更时排序，避免BuildContext每次复制+排序。
    ContextContributors.Sort(
        [](const FContextContributorEntry& A,
           const FContextContributorEntry& B)
        {
            if (A.Priority != B.Priority)
            {
                return A.Priority > B.Priority;
            }
            return A.ContributorId.LexicalLess(B.ContributorId);
        });
    return Result;
}

bool UGamePlatformPresentationClientSubsystem::UnregisterContextContributor(
    const FGamePlatformPresentationRegistrationHandle& Handle)
{
    return Handle.IsValid() &&
        ContextContributors.RemoveAll(
            [&Handle](const FContextContributorEntry& Entry)
            {
                return Entry.Handle.Id == Handle.Id;
            }) > 0;
}

FGamePlatformPresentationRegistrationHandle
UGamePlatformPresentationClientSubsystem::RegisterCatalogFragment(
    const FGamePlatformPresentationCatalogFragment& Fragment)
{
    FGamePlatformPresentationRegistrationHandle Result;
    if (!Fragment.IsValid() ||
        CatalogFragments.Num() >= MaxPresentationCatalogFragments ||
        CatalogFragments.ContainsByPredicate(
            [&Fragment](const FCatalogFragmentEntry& Existing)
            {
                return Existing.Fragment.FragmentId == Fragment.FragmentId;
            }))
    {
        return Result;
    }

    FCatalogFragmentEntry Entry;
    Entry.Handle.Id = FGuid::NewGuid();
    Entry.Fragment = Fragment;
    Result = Entry.Handle;
    CatalogFragments.Add(MoveTemp(Entry));
    return Result;
}

bool UGamePlatformPresentationClientSubsystem::PreflightCatalogFragment(
    const FGamePlatformPresentationCatalogFragment& Fragment, FString& OutError) const
{
    check(IsInGameThread());
    OutError.Reset();
    if (!Fragment.IsValid() || CatalogFragments.Num() >= MaxPresentationCatalogFragments)
    { OutError = TEXT("目录片段无效或作用域容量已满。"); return false; }
    const auto QueriesCanOverlap = [](const FGamePlatformPresentationContextQuery& A, const FGamePlatformPresentationContextQuery& B)
    {
        const auto Compatible = [](FName X, FName Y) { return X.IsNone() || Y.IsNone() || X == Y; };
        return Compatible(A.ProjectId,B.ProjectId) && Compatible(A.HeroDefinitionId,B.HeroDefinitionId) &&
            Compatible(A.AbilityId,B.AbilityId) && Compatible(A.SkinId,B.SkinId) && Compatible(A.WorldId,B.WorldId) &&
            Compatible(A.ExperienceId,B.ExperienceId) && Compatible(A.RegionId,B.RegionId) &&
            Compatible(A.ArenaModeId,B.ArenaModeId) && Compatible(A.ContentPackId,B.ContentPackId) &&
            Compatible(A.PlatformId,B.PlatformId) && (A.QualityTier == EGamePlatformPresentationQualityTier::Unknown ||
            B.QualityTier == EGamePlatformPresentationQualityTier::Unknown || A.QualityTier == B.QualityTier);
    };
    const auto Conflicts = [&](const auto& A, const auto& B)
    {
        return A.SemanticTag == B.SemanticTag && A.Scope == B.Scope && A.Priority == B.Priority &&
            A.ContextQuery.GetSpecificity() == B.ContextQuery.GetSpecificity() && QueriesCanOverlap(A.ContextQuery,B.ContextQuery);
    };
    for (int32 Index = 0; Index < Fragment.Entries.Num(); ++Index)
    {
        const auto& Candidate = Fragment.Entries[Index];
        for (int32 Other = 0; Other < Index; ++Other)
            if (Conflicts(Candidate, Fragment.Entries[Other]))
            {
                OutError = FString::Printf(TEXT("目录同键资格冲突：Pack=%s Catalog=%s Entries=%s/%s。"),
                    *Fragment.OwnerScopeId.ToString(), *Fragment.FragmentId.ToString(),
                    *Candidate.EntryId.ToString(), *Fragment.Entries[Other].EntryId.ToString());
                return false;
            }
        for (const auto& Existing : CatalogFragments)
        {
            if (Existing.Fragment.FragmentId == Fragment.FragmentId)
            { OutError = TEXT("目录身份已经登记，拒绝重复或版本漂移。"); return false; }
            for (const auto& Entry : Existing.Fragment.Entries)
                if (Conflicts(Candidate, Entry))
                {
                    OutError = FString::Printf(TEXT("目录同键资格冲突：Pack=%s Catalog=%s Entry=%s；Pack=%s Catalog=%s Entry=%s。"),
                        *Fragment.OwnerScopeId.ToString(), *Fragment.FragmentId.ToString(), *Candidate.EntryId.ToString(),
                        *Existing.Fragment.OwnerScopeId.ToString(), *Existing.Fragment.FragmentId.ToString(), *Entry.EntryId.ToString());
                    return false;
                }
        }
    }
    return true;
}

bool UGamePlatformPresentationClientSubsystem::UnregisterCatalogFragment(
    const FGamePlatformPresentationRegistrationHandle& Handle)
{
    return Handle.IsValid() &&
        CatalogFragments.RemoveAll(
            [&Handle](const FCatalogFragmentEntry& Entry)
            {
                return Entry.Handle.Id == Handle.Id;
            }) > 0;
}

bool UGamePlatformPresentationClientSubsystem::BuildContext(
    FGamePlatformPresentationContext& InOutContext) const
{
    // ContextContributors在注册时已保持Priority→ContributorId确定性顺序。
    for (const FContextContributorEntry& Entry : ContextContributors)
    {
        if (!Entry.Contributor.IsValid())
        {
            continue;
        }
        if (!ApplyPatch(
                InOutContext,
                Entry.Contributor->BuildPatch(),
                Entry.Contributor->GetConflictPolicy()))
        {
            return false;
        }
    }
    return true;
}

EGamePlatformPresentationCatalogResolveResult
UGamePlatformPresentationClientSubsystem::ResolveCatalog(
    const FGameplayTag& SemanticTag,
    const FGamePlatformPresentationContext& Context,
    FGamePlatformPresentationResolvedEntry& OutResolved) const
{
    OutResolved = FGamePlatformPresentationResolvedEntry();
    if (!SemanticTag.IsValid())
    {
        return EGamePlatformPresentationCatalogResolveResult::NoMatch;
    }

    const FGamePlatformPresentationCatalogEntry* BestEntry = nullptr;
    const FGamePlatformPresentationCatalogFragment* BestFragment = nullptr;
    FGamePlatformPresentationCatalogScore BestScore;
    bool bAmbiguous = false;

    for (const FCatalogFragmentEntry& FragmentEntry : CatalogFragments)
    {
        for (const FGamePlatformPresentationCatalogEntry& Entry :
             FragmentEntry.Fragment.Entries)
        {
            int32 SemanticRank = 0;
            int32 SemanticDistance = 0;
            if (SemanticTag == Entry.SemanticTag)
            {
                SemanticRank = 2;
            }
            else if (Entry.bAllowParentFallback && SemanticTag.MatchesTag(Entry.SemanticTag))
            {
                SemanticRank = 1;
                // 按直接父级逐层计算距离；同语义层级才进入Scope排序。
                FGameplayTag Parent = SemanticTag;
                while (Parent.IsValid() && Parent != Entry.SemanticTag)
                {
                    Parent = Parent.RequestDirectParent();
                    ++SemanticDistance;
                }
            }
            else
            {
                continue;
            }

            if (!Entry.ContextQuery.Matches(Context))
            {
                continue;
            }

            FGamePlatformPresentationCatalogScore Score;
            Score.SemanticRank = SemanticRank;
            Score.SemanticDistance = SemanticDistance;
            Score.Specificity =
                Entry.ContextQuery.GetSpecificity();
            Score.Scope = static_cast<int32>(Entry.Scope);
            Score.Priority = Entry.Priority;

            const auto& Query = Entry.ContextQuery;
            const auto DescribeCandidate = [&]() { return FString::Printf(TEXT("Pack=%s Catalog=%s Entry=%s Semantic=%s Hero=%s Ability=%s Skin=%s World=%s Platform=%s Quality=%d Project=%s Experience=%s Region=%s Arena=%s ContentPack=%s"),
                *FragmentEntry.Fragment.OwnerScopeId.ToString(), *FragmentEntry.Fragment.FragmentId.ToString(), *Entry.EntryId.ToString(),
                *Entry.SemanticTag.ToString(), *Query.HeroDefinitionId.ToString(), *Query.AbilityId.ToString(), *Query.SkinId.ToString(),
                *Query.WorldId.ToString(), *Query.PlatformId.ToString(), static_cast<int32>(Query.QualityTier), *Query.ProjectId.ToString(),
                *Query.ExperienceId.ToString(), *Query.RegionId.ToString(), *Query.ArenaModeId.ToString(), *Query.ContentPackId.ToString()); };
            if (!BestEntry || Score.IsBetterThan(BestScore))
            {
                BestEntry = &Entry; BestFragment = &FragmentEntry.Fragment;
                BestScore = Score;
                bAmbiguous = false;
                OutResolved.AmbiguousCandidates = {DescribeCandidate()};
            }
            else if (Score.IsEquivalentTo(BestScore))
            {
                bAmbiguous = true;
                OutResolved.AmbiguousCandidates.Add(DescribeCandidate());
            }
        }
    }

    if (bAmbiguous)
    {
        return EGamePlatformPresentationCatalogResolveResult::Ambiguous;
    }
    if (!BestEntry)
    {
        return EGamePlatformPresentationCatalogResolveResult::NoMatch;
    }

    OutResolved.AmbiguousCandidates.Reset();
    OutResolved.CatalogFragmentId = BestFragment->FragmentId; OutResolved.OwnerScopeId = BestFragment->OwnerScopeId;
    OutResolved.CatalogRevision = BestFragment->Revision; OutResolved.MatchedSemanticTag = BestEntry->SemanticTag;
    OutResolved.EntryId = BestEntry->EntryId;
    OutResolved.ProviderChannel = BestEntry->ProviderChannel;
    OutResolved.DefinitionId = BestEntry->DefinitionId;
    OutResolved.ContentRevision = BestEntry->ContentRevision;
    OutResolved.Scope = BestEntry->Scope;
    return EGamePlatformPresentationCatalogResolveResult::Resolved;
}


void UGamePlatformPresentationClientSubsystem::RefreshWorldGeneration()
{
    UWorld* CurrentWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (CurrentWorld != BoundWorld.Get())
    {
        BoundWorld = CurrentWorld;
        ++WorldGeneration;
    }
}

EGamePlatformPresentationSubmitResult UGamePlatformPresentationClientSubsystem::Submit(
    const FGamePlatformPresentationRequest& Request)
{
    RefreshWorldGeneration();
    if (!Request.IsValid())
    {
        RequestObserved.Broadcast(Request, EGamePlatformPresentationSubmitResult::InvalidRequest);
        return EGamePlatformPresentationSubmitResult::InvalidRequest;
    }
    if (Request.WorldGeneration > 0 && Request.WorldGeneration != WorldGeneration)
    {
        RequestObserved.Broadcast(Request, EGamePlatformPresentationSubmitResult::StaleWorld);
        return EGamePlatformPresentationSubmitResult::StaleWorld;
    }

    FGamePlatformPresentationRequest ResolvedRequest = Request;
    if (ResolvedRequest.WorldGeneration == 0) ResolvedRequest.WorldGeneration = WorldGeneration;
    if (ResolvedRequest.Context.WorldGeneration == 0)
    {
        ResolvedRequest.Context.WorldGeneration = WorldGeneration;
    }
    if (ResolvedRequest.Context.WorldGeneration != WorldGeneration)
    {
        RequestObserved.Broadcast(
            ResolvedRequest,
            EGamePlatformPresentationSubmitResult::StaleWorld);
        return EGamePlatformPresentationSubmitResult::StaleWorld;
    }
    if (!BuildContext(ResolvedRequest.Context))
    {
        RequestObserved.Broadcast(
            ResolvedRequest,
            EGamePlatformPresentationSubmitResult::InvalidRequest);
        return EGamePlatformPresentationSubmitResult::InvalidRequest;
    }

    if (ResolvedRequest.DefinitionId.IsNone())
    {
        FGamePlatformPresentationResolvedEntry Resolved;
        const EGamePlatformPresentationCatalogResolveResult ResolveResult =
            ResolveCatalog(
                ResolvedRequest.SemanticTag,
                ResolvedRequest.Context,
                Resolved);
        if (ResolveResult == EGamePlatformPresentationCatalogResolveResult::Ambiguous)
        {
            RequestObserved.Broadcast(
                ResolvedRequest,
                EGamePlatformPresentationSubmitResult::InvalidRequest);
            return EGamePlatformPresentationSubmitResult::InvalidRequest;
        }
        if (ResolveResult == EGamePlatformPresentationCatalogResolveResult::Resolved)
        {
            ResolvedRequest.ProviderChannel = Resolved.ProviderChannel;
            ResolvedRequest.DefinitionId = Resolved.DefinitionId;
            ResolvedRequest.ContentRevision = Resolved.ContentRevision;
        }
    }

    for (FProviderEntry& Entry : Providers)
    {
        if (Entry.Handler.IsBound() && Entry.Handler.Execute(ResolvedRequest))
        {
            RequestObserved.Broadcast(
                ResolvedRequest,
                EGamePlatformPresentationSubmitResult::Submitted);
            return EGamePlatformPresentationSubmitResult::Submitted;
        }
    }

    RequestObserved.Broadcast(
        ResolvedRequest,
        EGamePlatformPresentationSubmitResult::ProviderMissing);
    return EGamePlatformPresentationSubmitResult::ProviderMissing;
}

void UGamePlatformPresentationClientSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool /*bSessionEnded*/,
    bool /*bCleanupResources*/)
{
    if (World && World == BoundWorld.Get())
    {
        BoundWorld.Reset();
        ++WorldGeneration;
        ContextContributors.RemoveAll(
            [](const FContextContributorEntry& Entry)
            {
                return Entry.Scope == EGamePlatformPresentationContextScope::World;
            });
        CatalogFragments.RemoveAll(
            [](const FCatalogFragmentEntry& Entry)
            {
                return Entry.Fragment.LifecycleScope ==
                    EGamePlatformPresentationContextScope::World;
            });
    }
}
