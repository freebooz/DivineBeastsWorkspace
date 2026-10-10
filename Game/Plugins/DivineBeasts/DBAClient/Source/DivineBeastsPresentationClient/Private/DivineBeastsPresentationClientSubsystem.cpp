// 本文件属于DivineBeasts项目层 DivineBeastsPresentationClient，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 项目本地玩家表现协调：Data真实预载成功后原子发布目录；只拥有本次租约与注册，不执行VFX或玩法。
#include "DivineBeastsPresentationClientSubsystem.h"

#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Context/DivineBeastsProjectContextContributor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformId.h"
#include "Engine/World.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"
#include "UObject/UObjectGlobals.h"

void UDivineBeastsPresentationClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ScopeId = FGuid::NewGuid(); bClosing = false;
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

    bClosing = true;
    UnregisterProjectState();
    CancelAllLogicalPreloads();
    ActivePacks.Reset(); PendingPacks.Reset();
    PackTerminals.Reset(); PackTerminalOrder.Reset(); ContentPackChanged.Clear();
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

    BeginDefaultCatalogPreload();
}

void UDivineBeastsPresentationClientSubsystem::BeginDefaultCatalogPreload()
{
    if (bClosing || !PlatformPresentation) return;
    // 默认条目是已规划的逻辑合同；缺真实Definition时只报告配置缺失，不发布可解析伪成功。
    TArray<FName> DefaultIds;
    for (const auto& Entry : FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment().Entries)
        DefaultIds.AddUnique(Entry.DefinitionId);
    DefaultCatalogError = TEXT("默认公共表现Definition尚未预载成功；请交付并登记真实资产。");
    DefaultPreloadRequestId = BeginLogicalPreload(FDivineBeastsProjectCatalog::GetProjectId(), DefaultIds, true, {});
}

void UDivineBeastsPresentationClientSubsystem::UnregisterProjectState()
{
    if (PlatformPresentation)
    {
        TArray<FDivineBeastsPresentationContentPackHandle> Handles;
        for (const auto& Pair : ActivePacks) Handles.Add(Pair.Value.Handle);
        for (const auto& Pair : PendingPacks) Handles.Add(Pair.Value.Handle);
        for (const auto& Handle : Handles) DeactivateContentPack(Handle);

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
    TArray<FDivineBeastsPresentationContentPackHandle> ActiveHandles;
    for (const auto& Pair : ActivePacks) ActiveHandles.Add(Pair.Value.Handle);
    for (const auto& Handle : ActiveHandles) DeactivateContentPack(Handle);
    TArray<FDivineBeastsPresentationContentPackHandle> PendingHandles;
    for (const auto& Pair : PendingPacks) PendingHandles.Add(Pair.Value.Handle);
    for (const auto& Handle : PendingHandles) DeactivateContentPack(Handle);
    if (PlatformPresentation) PlatformPresentation->UnregisterCatalogFragment(DefaultCatalogHandle);
    DefaultCatalogHandle = {}; DefaultPreloadRequestId = {};
    CancelAllLogicalPreloads();

    ProjectContext = FDivineBeastsPresentationProjectContext();
    ProjectContext.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    RequestStates.Reset();
    RequestOrder.Reset();
    BeginDefaultCatalogPreload();
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
    const FDivineBeastsPresentationContentPackFragment& Fragment, FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    if (bClosing || !PlatformPresentation || !Fragment.IsValid(OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("项目表现服务已关闭或平台目录不可用。");
        return {};
    }
    for (const auto& Pair : ActivePacks)
        if (Pair.Value.ContentPackId == Fragment.ContentPackId)
        { OutError = TEXT("同一内容包已经激活，拒绝版本漂移。"); return {}; }
    for (const auto& Pair : PendingPacks)
        if (Pair.Value.Fragment.ContentPackId == Fragment.ContentPackId)
        { OutError = TEXT("同一内容包正在加载，拒绝重复选择。"); return {}; }
    FPendingPack Pending;
    Pending.Handle.Id = FGuid::NewGuid(); Pending.Handle.ScopeId = ScopeId;
    Pending.Handle.Generation = ++NextPreloadGeneration;
    Pending.Fragment = Fragment;
    const auto Handle = Pending.Handle;
    PendingPacks.Add(Handle.Id, MoveTemp(Pending));
    TArray<FName> DefinitionIds = Fragment.LogicalPreloadDefinitionIds;
    for (const auto& Entry : Fragment.CatalogFragment.Entries) DefinitionIds.AddUnique(Entry.DefinitionId);
    const FGuid PreloadId = BeginLogicalPreload(Fragment.ContentPackId, DefinitionIds, true, Handle);
    if (!PreloadId.IsValid())
    {
        PendingPacks.Remove(Handle.Id);
        OutError = TEXT("内容包的Definition身份、所属世界或Data预载申请无效；目录未发布。");
        RecordPackTerminal(Handle, EDivineBeastsPresentationContentPackState::Failed, OutError);
        return {};
    }
    return Handle; // 只表示Loading受理；GetContentPackState/通知确认Active。
}

bool UDivineBeastsPresentationClientSubsystem::DeactivateContentPack(
    const FDivineBeastsPresentationContentPackHandle& Handle)
{
    check(IsInGameThread());
    if (!Handle.IsValid() || Handle.ScopeId != ScopeId) return false;
    if (const auto* Pending = PendingPacks.Find(Handle.Id))
    {
        if (Pending->Handle.Generation != Handle.Generation) return false;
        PendingPacks.Remove(Handle.Id);
        CancelLogicalPreload(Handle.Id);
        RecordPackTerminal(Handle, EDivineBeastsPresentationContentPackState::Cancelled, TEXT("内容包在发布前取消。"));
        return true;
    }
    const auto* Stored = ActivePacks.Find(Handle.Id);
    if (!Stored || Stored->Handle.Generation != Handle.Generation) return false;
    const FActivePack Active = *Stored;
    ActivePacks.Remove(Handle.Id); // 先撤销事务所有权，广播重入不能重复释放。
    if (PlatformPresentation) PlatformPresentation->UnregisterCatalogFragment(Active.CatalogHandle);
    for (const auto& Id : Active.PreloadRequestIds) CancelLogicalPreload(Id);
    RecordPackTerminal(Handle, EDivineBeastsPresentationContentPackState::Deactivated, TEXT("目录已撤销，自有预载租约已释放。"));
    return true;
}

EDivineBeastsPresentationContentPackState UDivineBeastsPresentationClientSubsystem::GetContentPackState(
    const FDivineBeastsPresentationContentPackHandle& Handle, FString& OutError) const
{
    OutError.Reset();
    if (!Handle.IsValid() || Handle.ScopeId != ScopeId) return EDivineBeastsPresentationContentPackState::Invalid;
    if (const auto* Pending = PendingPacks.Find(Handle.Id))
        if (Pending->Handle.Generation == Handle.Generation) return EDivineBeastsPresentationContentPackState::Loading;
    if (const auto* Active = ActivePacks.Find(Handle.Id))
        if (Active->Handle.Generation == Handle.Generation) return EDivineBeastsPresentationContentPackState::Active;
    if (const auto* Terminal = PackTerminals.Find(Handle.Id))
        if (Terminal->Handle.Generation == Handle.Generation) { OutError = Terminal->Error; return Terminal->State; }
    return EDivineBeastsPresentationContentPackState::Invalid;
}

FGuid UDivineBeastsPresentationClientSubsystem::RequestLogicalPreload(FName OwnerScopeId,
    const TArray<FName>& DefinitionIds, bool bRequired)
{
    return BeginLogicalPreload(OwnerScopeId, DefinitionIds, bRequired, {});
}

FGuid UDivineBeastsPresentationClientSubsystem::BeginLogicalPreload(FName OwnerScopeId,
    const TArray<FName>& DefinitionIds, bool bRequired, const FDivineBeastsPresentationContentPackHandle& PackHandle)
{
    if (bClosing || OwnerScopeId.IsNone() || DefinitionIds.IsEmpty()) return {};
    auto* Player = GetLocalPlayer(); auto* GameInstance = Player ? Player->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data) return {};
    FLogicalPreload Request;
    Request.RequestId = PackHandle.IsValid() ? PackHandle.Id : FGuid::NewGuid();
    Request.Generation = ++NextPreloadGeneration; Request.OwnerScopeId = OwnerScopeId;
    Request.bRequired = bRequired; Request.PackHandle = PackHandle;
    TArray<FPrimaryAssetId> AssetIds;
    TSet<FName> Unique;
    for (const FName DefinitionId : DefinitionIds)
    {
        FGamePlatformId Id;
        if (!FGamePlatformId::TryParse(DefinitionId.ToString(), Id)) return {};
        const FName Canonical(*Id.ToString());
        if (Unique.Contains(Canonical)) return {};
        Unique.Add(Canonical); Request.DefinitionIds.Add(Canonical);
        AssetIds.Emplace(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), Canonical);
    }
    Request.RemainingLoads = AssetIds.Num();
    const FGuid RequestId = Request.RequestId; const int64 Generation = Request.Generation;
    LogicalPreloads.Add(RequestId, Request);
    const TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> WeakThis(this);
    const auto Lifetime = PackHandle.IsValid() && PendingPacks.FindChecked(PackHandle.Id).Fragment.LifecycleScope == EGamePlatformPresentationContextScope::World
        ? EGamePlatformDataLifetime::World : EGamePlatformDataLifetime::Instance;
    for (const auto& AssetId : AssetIds)
    {
        FGamePlatformResult Accepted;
        const auto Lease = Data->AcquireDefinition(AssetId, UGamePlatformDefinitionBase::StaticClass(),
            {TEXT("VFXRuntime"), TEXT("SFXRuntime")}, Lifetime, this,
            [WeakThis, RequestId, Generation](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
            {
                if (auto* Self = WeakThis.Get()) Self->HandleLogicalPreloadCompleted(RequestId, Generation, CompletedLease, Result);
            }, Accepted);
        if (!Accepted.IsSuccess() || !Lease.IsValid())
        {
            PendingPacks.Remove(PackHandle.Id);
            CancelLogicalPreload(RequestId);
            return {};
        }
        if (auto* Stored = LogicalPreloads.Find(RequestId)) Stored->Leases.Add(Lease);
        else { Data->ReleaseDefinition(Lease); return {}; }
    }
    // 广播只作诊断，Data才是完成依据；传值快照防止观察者取消后读取已销毁的数组。
    const auto Snapshot = Request.DefinitionIds;
    LogicalPreloadRequested.Broadcast(RequestId, OwnerScopeId, Snapshot, bRequired);
    return LogicalPreloads.Contains(RequestId) ? RequestId : FGuid();
}

void UDivineBeastsPresentationClientSubsystem::HandleLogicalPreloadCompleted(const FGuid RequestId,
    const int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    auto* Stored = LogicalPreloads.Find(RequestId);
    if (bClosing || !Stored || Stored->Generation != Generation || Stored->bSucceeded ||
        !Stored->Leases.ContainsByPredicate([&](const auto& Item) { return Item.LeaseId == Lease.LeaseId && Item.Generation == Lease.Generation && Item.ScopeId == Lease.ScopeId &&
        Item.IssuerProof == Lease.IssuerProof && Item.DefinitionId == Lease.DefinitionId; })) return;
    auto* Player = GetLocalPlayer(); auto* GameInstance = Player ? Player->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    const auto PackHandle = Stored->PackHandle;
    const auto* LoadedDefinition = Result.IsSuccess() && Data ? Data->GetLoadedDefinition(Lease) : nullptr;
    bool bProviderTypeValid = true;
    FGamePlatformPresentationCatalogFragment ExpectedCatalog;
    if (const auto* PendingPack = PendingPacks.Find(PackHandle.Id)) ExpectedCatalog = PendingPack->Fragment.CatalogFragment;
    else if (RequestId == DefaultPreloadRequestId) ExpectedCatalog = FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment();
    for (const auto& Entry : ExpectedCatalog.Entries)
        if (Entry.DefinitionId == Lease.DefinitionId.PrimaryAssetName)
        {
            UClass* ExpectedClass = PlatformPresentation ? PlatformPresentation->GetProviderDefinitionClass(Entry.ProviderChannel) : nullptr;
            if (!ExpectedClass || !LoadedDefinition || !LoadedDefinition->IsA(ExpectedClass)) bProviderTypeValid = false;
        }
    if (!bProviderTypeValid || !Result.IsSuccess() || !Data || Data->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded || !Data->GetLoadedDefinition(Lease))
    {
        if (RequestId == DefaultPreloadRequestId)
            DefaultCatalogError = TEXT("默认公共表现Definition未配置、实际资源预载失败或提供者/类型缺失；目录未发布。");
        PendingPacks.Remove(PackHandle.Id);
        CancelLogicalPreload(RequestId);
        if (PackHandle.IsValid()) RecordPackTerminal(PackHandle, EDivineBeastsPresentationContentPackState::Failed,
            TEXT("内容包必需Definition/资源加载失败，或提供者缺失/类型不符；本次全部租约已回滚。"));
        return;
    }
    // 同一Lease可能收到重复回执；移出等待集合但继续保留自有成功租约。
    const FGuid CompletedLeaseId = Lease.LeaseId;
    CompletedPreloadLeases.FindOrAdd(RequestId).Add(CompletedLeaseId);
    Stored->RemainingLoads = Stored->Leases.Num() - CompletedPreloadLeases.FindChecked(RequestId).Num();
    if (Stored->RemainingLoads > 0) return;
    Stored->bSucceeded = true;
    if (RequestId == DefaultPreloadRequestId && PlatformPresentation)
    {
        DefaultCatalogHandle = PlatformPresentation->RegisterCatalogFragment(FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment());
        if (DefaultCatalogHandle.IsValid()) DefaultCatalogError.Reset();
        else { DefaultCatalogError = TEXT("默认公共表现目录注册失败。"); CancelLogicalPreload(RequestId); }
        return;
    }
    if (!PackHandle.IsValid()) return; // 独立预载成功，保持租约直到调用者取消。
    const auto* Pending = PendingPacks.Find(PackHandle.Id);
    if (!Pending || Pending->Handle.Generation != PackHandle.Generation || !PlatformPresentation) { CancelLogicalPreload(RequestId); return; }
    const auto Fragment = Pending->Fragment;
    FString PreflightError;
    const auto CatalogHandle = PlatformPresentation->PreflightCatalogFragment(Fragment.CatalogFragment, PreflightError)
        ? PlatformPresentation->RegisterCatalogFragment(Fragment.CatalogFragment) : FGamePlatformPresentationRegistrationHandle();
    PendingPacks.Remove(PackHandle.Id);
    if (!CatalogHandle.IsValid())
    {
        CancelLogicalPreload(RequestId);
        RecordPackTerminal(PackHandle, EDivineBeastsPresentationContentPackState::Failed, TEXT("预载成功但目录预检/注册失败，租约已回滚。"));
        return;
    }
    FActivePack Active; Active.Handle = PackHandle; Active.ContentPackId = Fragment.ContentPackId;
    Active.LifecycleScope = Fragment.LifecycleScope; Active.CatalogHandle = CatalogHandle;
    Active.PreloadRequestIds.Add(RequestId); ActivePacks.Add(PackHandle.Id, MoveTemp(Active));
    const FString NoError;
    ContentPackChanged.Broadcast(PackHandle, EDivineBeastsPresentationContentPackState::Active, NoError);
}

void UDivineBeastsPresentationClientSubsystem::ReleaseLogicalLeases(const TArray<FGamePlatformDataLease>& Leases)
{
    auto* Player = GetLocalPlayer(); auto* GameInstance = Player ? Player->GetGameInstance() : nullptr;
    if (auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr)
        for (const auto& Lease : Leases) Data->ReleaseDefinition(Lease);
}

bool UDivineBeastsPresentationClientSubsystem::CancelLogicalPreload(const FGuid& RequestId)
{
    FLogicalPreload Removed;
    if (!LogicalPreloads.RemoveAndCopyValue(RequestId, Removed)) return false;
    CompletedPreloadLeases.Remove(RequestId);
    const bool bCancelledPack = PendingPacks.Remove(Removed.PackHandle.Id) > 0;
    ReleaseLogicalLeases(Removed.Leases);
    const FGuid SnapshotRequestId = RequestId;
    LogicalPreloadCancelled.Broadcast(SnapshotRequestId);
    if (bCancelledPack) RecordPackTerminal(Removed.PackHandle, EDivineBeastsPresentationContentPackState::Cancelled, TEXT("内容包逻辑预载由调用者取消。"));
    return true;
}

void UDivineBeastsPresentationClientSubsystem::RecordPackTerminal(const FDivineBeastsPresentationContentPackHandle& Handle,
    EDivineBeastsPresentationContentPackState State, const FString& Error)
{
    if (!Handle.IsValid() || PackTerminals.Contains(Handle.Id)) return;
    FPackTerminal Terminal; Terminal.Handle = Handle; Terminal.State = State; Terminal.Error = Error;
    if (PackTerminalOrder.Num() >= 128) { PackTerminals.Remove(PackTerminalOrder[0]); PackTerminalOrder.RemoveAt(0); }
    PackTerminalOrder.Add(Handle.Id); PackTerminals.Add(Handle.Id, MoveTemp(Terminal));
    const auto SnapshotHandle = Handle; const FString SnapshotError = Error;
    ContentPackChanged.Broadcast(SnapshotHandle, State, SnapshotError); // 本次所有权已结束，再开放重入。
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

void UDivineBeastsPresentationClientSubsystem::DeactivatePacksByScope(EGamePlatformPresentationContextScope Scope)
{
    TArray<FDivineBeastsPresentationContentPackHandle> Handles;
    for (const auto& Pair : ActivePacks) if (Pair.Value.LifecycleScope == Scope) Handles.Add(Pair.Value.Handle);
    for (const auto& Pair : PendingPacks) if (Pair.Value.Fragment.LifecycleScope == Scope) Handles.Add(Pair.Value.Handle);
    for (const auto& Handle : Handles) DeactivateContentPack(Handle);
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
    // 只有真正受理的事务才进入去重历史；缺目录/提供者等失败必须允许后续真实重试。
    if (Result != EGamePlatformPresentationSubmitResult::Submitted)
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
    if (Result != EGamePlatformPresentationSubmitResult::Submitted)
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
