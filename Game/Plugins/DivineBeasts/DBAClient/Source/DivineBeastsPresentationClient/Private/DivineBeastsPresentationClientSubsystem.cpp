#include "DivineBeastsPresentationClientSubsystem.h"

#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Context/DivineBeastsProjectContextContributor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"
#include "UObject/UObjectGlobals.h"

void UDivineBeastsPresentationClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformPresentationClientSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformPresentation =
            LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    }

    ProjectContext.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    RegisterProjectState();

    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UDivineBeastsPresentationClientSubsystem::HandleWorldCleanup);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &UDivineBeastsPresentationClientSubsystem::HandlePostLoadMap);
}

void UDivineBeastsPresentationClientSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    UnregisterProjectState();
    CancelAllLogicalPreloads();
    ActivePacks.Reset();
    RequestStates.Reset();
    RequestOrder.Reset();
    PlatformPresentation = nullptr;
    Super::Deinitialize();
}

void UDivineBeastsPresentationClientSubsystem::RegisterProjectState()
{
    if (!PlatformPresentation)
    {
        return;
    }

    ContextContributorHandle =
        PlatformPresentation->RegisterContextContributor(
            MakeShared<FDivineBeastsProjectContextContributor>(this));

    DefaultCatalogHandle =
        PlatformPresentation->RegisterCatalogFragment(
            FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment());
}

void UDivineBeastsPresentationClientSubsystem::UnregisterProjectState()
{
    if (PlatformPresentation)
    {
        TArray<FGuid> PackIds;
        ActivePacks.GetKeys(PackIds);
        for (const FGuid& Id : PackIds)
        {
            FDivineBeastsPresentationContentPackHandle Handle;
            Handle.Id = Id;
            DeactivateContentPack(Handle);
        }

        PlatformPresentation->UnregisterCatalogFragment(DefaultCatalogHandle);
        PlatformPresentation->UnregisterContextContributor(
            ContextContributorHandle);
    }

    ContextContributorHandle = {};
    DefaultCatalogHandle = {};
}

bool UDivineBeastsPresentationClientSubsystem::UpdateProjectContext(
    const FDivineBeastsPresentationProjectContext& Context,
    FString& OutError)
{
    FDivineBeastsPresentationProjectContext Normalized = Context;
    if (Normalized.ProjectId.IsNone())
    {
        Normalized.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    }
    if (!Normalized.IsValid(OutError))
    {
        return false;
    }

    const bool bCharacterScopeChanged =
        ProjectContext.HeroDefinitionId != Normalized.HeroDefinitionId ||
        ProjectContext.SkinId != Normalized.SkinId ||
        ProjectContext.AvatarGeneration != Normalized.AvatarGeneration;

    const bool bWorldScopeChanged =
        ProjectContext.WorldId != Normalized.WorldId ||
        ProjectContext.ExperienceId != Normalized.ExperienceId ||
        ProjectContext.RegionId != Normalized.RegionId ||
        ProjectContext.WorldGeneration != Normalized.WorldGeneration;

    if (bCharacterScopeChanged)
    {
        DeactivatePacksByScope(
            EGamePlatformPresentationContextScope::LocalPlayer);
    }
    if (bWorldScopeChanged)
    {
        DeactivatePacksByScope(EGamePlatformPresentationContextScope::World);
    }

    ProjectContext = MoveTemp(Normalized);
    if (bCharacterScopeChanged || bWorldScopeChanged)
    {
        RequestStates.Reset();
        RequestOrder.Reset();
    }
    return true;
}

void UDivineBeastsPresentationClientSubsystem::ResetForAccountSwitch()
{
    TArray<FGuid> PackIds;
    ActivePacks.GetKeys(PackIds);
    for (const FGuid& Id : PackIds)
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        Handle.Id = Id;
        DeactivateContentPack(Handle);
    }
    CancelAllLogicalPreloads();

    ProjectContext = FDivineBeastsPresentationProjectContext();
    ProjectContext.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    RequestStates.Reset();
    RequestOrder.Reset();
}

FGamePlatformPresentationContextPatch
UDivineBeastsPresentationClientSubsystem::BuildProjectContextPatch() const
{
    FGamePlatformPresentationContextPatch Patch;
    Patch.Values = ProjectContext.ToPlatformContext();
    return Patch;
}

FDivineBeastsPresentationContentPackHandle
UDivineBeastsPresentationClientSubsystem::ActivateContentPack(
    const FDivineBeastsPresentationContentPackFragment& Fragment,
    FString& OutError)
{
    FDivineBeastsPresentationContentPackHandle Result;
    if (!PlatformPresentation || !Fragment.IsValid(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("GamePlatformPresentationClient不可用。");
        }
        return Result;
    }

    for (const TPair<FGuid, FActivePack>& Pair : ActivePacks)
    {
        if (Pair.Value.ContentPackId == Fragment.ContentPackId)
        {
            OutError = TEXT("同一ContentPackId已经激活，拒绝重复/版本漂移注册。");
            return Result;
        }
    }

    const FGamePlatformPresentationRegistrationHandle CatalogHandle =
        PlatformPresentation->RegisterCatalogFragment(Fragment.CatalogFragment);
    if (!CatalogHandle.IsValid())
    {
        OutError = TEXT("ContentPack Catalog注册失败。");
        return Result;
    }

    FActivePack Active;
    Active.Handle.Id = FGuid::NewGuid();
    Active.ContentPackId = Fragment.ContentPackId;
    Active.LifecycleScope = Fragment.LifecycleScope;
    Active.CatalogHandle = CatalogHandle;

    if (!Fragment.LogicalPreloadDefinitionIds.IsEmpty())
    {
        const FGuid PreloadId = RequestLogicalPreload(
            Fragment.ContentPackId,
            Fragment.LogicalPreloadDefinitionIds,
            Fragment.bRequiredPreload);
        if (PreloadId.IsValid())
        {
            Active.PreloadRequestIds.Add(PreloadId);
        }
    }

    Result = Active.Handle;
    ActivePacks.Add(Result.Id, MoveTemp(Active));
    return Result;
}

bool UDivineBeastsPresentationClientSubsystem::DeactivateContentPack(
    const FDivineBeastsPresentationContentPackHandle& Handle)
{
    if (!Handle.IsValid())
    {
        return false;
    }

    FActivePack* Active = ActivePacks.Find(Handle.Id);
    if (!Active)
    {
        return false;
    }

    const TArray<FGuid> PreloadIds = Active->PreloadRequestIds;
    for (const FGuid& PreloadId : PreloadIds)
    {
        CancelLogicalPreload(PreloadId);
    }

    if (PlatformPresentation)
    {
        PlatformPresentation->UnregisterCatalogFragment(Active->CatalogHandle);
    }

    ActivePacks.Remove(Handle.Id);
    return true;
}

FGuid UDivineBeastsPresentationClientSubsystem::RequestLogicalPreload(
    FName OwnerScopeId,
    const TArray<FName>& DefinitionIds,
    bool bRequired)
{
    if (OwnerScopeId.IsNone() || DefinitionIds.IsEmpty())
    {
        return FGuid();
    }

    TSet<FName> Unique;
    FLogicalPreload Request;
    Request.RequestId = FGuid::NewGuid();
    Request.OwnerScopeId = OwnerScopeId;
    Request.bRequired = bRequired;

    for (const FName DefinitionId : DefinitionIds)
    {
        if (DefinitionId.IsNone() || Unique.Contains(DefinitionId))
        {
            return FGuid();
        }
        Unique.Add(DefinitionId);
        Request.DefinitionIds.Add(DefinitionId);
    }

    const FGuid Result = Request.RequestId;
    LogicalPreloads.Add(Result, Request);
    LogicalPreloadRequested.Broadcast(
        Result,
        OwnerScopeId,
        Request.DefinitionIds,
        bRequired);
    return Result;
}

bool UDivineBeastsPresentationClientSubsystem::CancelLogicalPreload(
    const FGuid& RequestId)
{
    if (!RequestId.IsValid() || !LogicalPreloads.Contains(RequestId))
    {
        return false;
    }

    LogicalPreloads.Remove(RequestId);
    LogicalPreloadCancelled.Broadcast(RequestId);
    return true;
}

void UDivineBeastsPresentationClientSubsystem::CancelAllLogicalPreloads()
{
    TArray<FGuid> Ids;
    LogicalPreloads.GetKeys(Ids);
    for (const FGuid& Id : Ids)
    {
        CancelLogicalPreload(Id);
    }
}

void UDivineBeastsPresentationClientSubsystem::DeactivatePacksByScope(
    EGamePlatformPresentationContextScope Scope)
{
    TArray<FGuid> Ids;
    for (const TPair<FGuid, FActivePack>& Pair : ActivePacks)
    {
        if (Pair.Value.LifecycleScope == Scope)
        {
            Ids.Add(Pair.Key);
        }
    }

    Ids.Sort(
        [](const FGuid& A, const FGuid& B)
        {
            return A.ToString(EGuidFormats::Digits) <
                   B.ToString(EGuidFormats::Digits);
        });
    for (const FGuid& Id : Ids)
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        Handle.Id = Id;
        DeactivateContentPack(Handle);
    }
}

bool UDivineBeastsPresentationClientSubsystem::ShouldDispatch(
    const FGuid& RequestId,
    EGamePlatformPresentationPredictionState State)
{
    if (!RequestId.IsValid())
    {
        return false;
    }

    if (EGamePlatformPresentationPredictionState* Existing =
        RequestStates.Find(RequestId))
    {
        if (*Existing == State)
        {
            return false;
        }
        if (*Existing == EGamePlatformPresentationPredictionState::Predicted &&
            State == EGamePlatformPresentationPredictionState::Confirmed)
        {
            *Existing = State;
            return false;
        }
        if (*Existing == EGamePlatformPresentationPredictionState::Confirmed &&
            State == EGamePlatformPresentationPredictionState::Predicted)
        {
            return false;
        }

        *Existing = State;
        return true;
    }

    RequestStates.Add(RequestId, State);
    RequestOrder.Add(RequestId);
    constexpr int32 MaxRememberedRequests = 2048;
    while (RequestOrder.Num() > MaxRememberedRequests)
    {
        const FGuid Oldest = RequestOrder[0];
        RequestOrder.RemoveAt(0);
        RequestStates.Remove(Oldest);
    }
    return true;
}

EGamePlatformPresentationSubmitResult
UDivineBeastsPresentationClientSubsystem::SubmitProjectRequest(
    FGamePlatformPresentationRequest Request)
{
    return PlatformPresentation
        ? PlatformPresentation->Submit(Request)
        : EGamePlatformPresentationSubmitResult::ProviderMissing;
}

EGamePlatformPresentationSubmitResult
UDivineBeastsPresentationClientSubsystem::SubmitWorldInteractionFact(
    const FDivineBeastsWorldInteractionPresentationFact& Fact)
{
    if (!Fact.IsValid())
    {
        return EGamePlatformPresentationSubmitResult::InvalidRequest;
    }
    if (!ShouldDispatch(Fact.FactId, Fact.PredictionState))
    {
        return EGamePlatformPresentationSubmitResult::Submitted;
    }

    FGamePlatformPresentationRequest Request;
    Request.RequestId = Fact.FactId;
    Request.SemanticTag =
        DivineBeastsPresentationTags::World_Interaction_Committed;
    Request.ContextId = Fact.OptionId;
    Request.SourceId = Fact.OptionId;
    Request.WorldGeneration = Fact.WorldGeneration;
    Request.RequestGeneration = Fact.AvatarGeneration;
    Request.PredictionState = Fact.PredictionState;
    Request.Context.HeroDefinitionId = Fact.HeroDefinitionId;
    Request.Context.WorldId = Fact.WorldId;
    Request.Context.ExperienceId = Fact.ExperienceId;
    Request.Context.RegionId = Fact.RegionId;
    Request.Context.WorldGeneration = Fact.WorldGeneration;
    Request.Context.AvatarGeneration = Fact.AvatarGeneration;

    const EGamePlatformPresentationSubmitResult Result =
        SubmitProjectRequest(MoveTemp(Request));
    if (Result == EGamePlatformPresentationSubmitResult::InvalidRequest ||
        Result == EGamePlatformPresentationSubmitResult::StaleWorld)
    {
        RequestStates.Remove(Fact.FactId);
        RequestOrder.RemoveSingle(Fact.FactId);
    }
    return Result;
}

EGamePlatformPresentationSubmitResult
UDivineBeastsPresentationClientSubsystem::SubmitVillageFeedbackFact(
    const FDivineBeastsVillageFeedbackPresentationFact& Fact)
{
    if (!Fact.IsValid())
    {
        return EGamePlatformPresentationSubmitResult::InvalidRequest;
    }
    if (!ShouldDispatch(Fact.FactId, Fact.PredictionState))
    {
        return EGamePlatformPresentationSubmitResult::Submitted;
    }

    FGamePlatformPresentationRequest Request;
    Request.RequestId = Fact.FactId;
    Request.SemanticTag =
        DivineBeastsPresentationTags::Village_Guidance_Ready;
    Request.ContextId = Fact.FeedbackId;
    Request.SourceId = Fact.FeedbackId;
    Request.WorldGeneration = Fact.WorldGeneration;
    Request.RequestGeneration = Fact.AvatarGeneration;
    Request.PredictionState = Fact.PredictionState;
    Request.Context.HeroDefinitionId = Fact.HeroDefinitionId;
    Request.Context.WorldId = Fact.WorldId;
    Request.Context.ExperienceId = Fact.ExperienceId;
    Request.Context.RegionId = Fact.RegionId;
    Request.Context.WorldGeneration = Fact.WorldGeneration;
    Request.Context.AvatarGeneration = Fact.AvatarGeneration;

    const EGamePlatformPresentationSubmitResult Result =
        SubmitProjectRequest(MoveTemp(Request));
    if (Result == EGamePlatformPresentationSubmitResult::InvalidRequest ||
        Result == EGamePlatformPresentationSubmitResult::StaleWorld)
    {
        RequestStates.Remove(Fact.FactId);
        RequestOrder.RemoveSingle(Fact.FactId);
    }
    return Result;
}

void UDivineBeastsPresentationClientSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool /*bSessionEnded*/,
    bool /*bCleanupResources*/)
{
    UWorld* LocalWorld =
        GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (!World || World != LocalWorld)
    {
        return;
    }

    DeactivatePacksByScope(EGamePlatformPresentationContextScope::World);
    ProjectContext.WorldId = NAME_None;
    ProjectContext.ExperienceId = NAME_None;
    ProjectContext.RegionId = NAME_None;
    ProjectContext.ArenaModeId = NAME_None;
    ProjectContext.ContentPackId = NAME_None;
    ProjectContext.WorldGeneration = 0;
    RequestStates.Reset();
    RequestOrder.Reset();
}

void UDivineBeastsPresentationClientSubsystem::HandlePostLoadMap(UWorld* World)
{
    UWorld* LocalWorld =
        GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (World && World == LocalWorld)
    {
        ProjectContext.WorldGeneration = 0;
        RequestStates.Reset();
        RequestOrder.Reset();
    }
}
