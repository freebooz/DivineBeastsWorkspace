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
    FGamePlatformPresentationProviderHandler Handler)
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
                Entry.Specificity + Entry.ContextQuery.GetSpecificity();
            Score.Scope = static_cast<int32>(Entry.Scope);
            Score.Priority = Entry.Priority;

            if (!BestEntry || Score.IsBetterThan(BestScore))
            {
                BestEntry = &Entry;
                BestScore = Score;
                bAmbiguous = false;
            }
            else if (Score.IsEquivalentTo(BestScore))
            {
                bAmbiguous = true;
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
