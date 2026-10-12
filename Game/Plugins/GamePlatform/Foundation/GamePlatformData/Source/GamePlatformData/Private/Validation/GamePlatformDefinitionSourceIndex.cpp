// 平台层源查重生产实现；仅元数据和值类型缓存，所有入口和Registry广播均在GameThread。
// 实际加载、外部AssetManager基线、租约与世界生命周期继续归现有Data加载服务。
#include "Validation/GamePlatformDefinitionSourceIndex.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Templates/UnrealTemplate.h"

namespace GamePlatform::Data
{
namespace
{
/** 只解析合法源标签；不把文件名、已去重主资产表或非法字符串替代稳定LogicalId。 */
bool ReadSourceIdentity(const FAssetData& Asset, FGamePlatformId& OutId)
{
    FString LogicalText;
    return Asset.GetTagValue(UGamePlatformPrimaryDataAsset::LogicalIdTag(), LogicalText) &&
        FGamePlatformId::TryParse(LogicalText, OutId);
}

/** 已加载类直接查继承；未加载类交给Registry递归类过滤，禁止为查重同步加载蓝图类。 */
bool IsPlatformDefinitionAsset(IAssetRegistry& Registry, const FAssetData& Asset)
{
    const UClass* BaseClass = UGamePlatformPrimaryDataAsset::StaticClass();
    if (const UClass* AssetClass = Asset.GetClass(EResolveClass::No))
        return AssetClass->IsChildOf(BaseClass);

    FARFilter Filter;
    Filter.ClassPaths.Add(BaseClass->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> CandidateAssets{Asset};
    // RunAssetsThroughFilter仅筛选所给单条元数据，不枚举全库；支持尚未加载的派生类。
    Registry.RunAssetsThroughFilter(CandidateAssets, Filter);
    return !CandidateAssets.IsEmpty();
}

/** 蓝图生成类/父类变化可能改变其他源的资格，必须按完整新类关系重新取快照。 */
bool CanChangeClassHierarchy(const FAssetData& Asset)
{
    FString GeneratedClassPath;
    return Asset.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassPath);
}
}

FDefinitionSourceIndex::~FDefinitionSourceIndex()
{
    // 模块正常Shutdown已完成清理时，静态析构不再访问引擎线程/Registry状态。
    if (bActive || BoundRegistry) Shutdown();
}

void FDefinitionSourceIndex::Startup(IAssetRegistry& InRegistry)
{
    check(IsInGameThread());
    FDefinitionSourceIndexAccess RegistryAccess;
    RegistryAccess.IsDiscovering = [&InRegistry]()
    {
        // UE5.8的IsLoadingAssets只覆盖首次扫描；后续挂载须同时检查IsGathering。
        return InRegistry.IsLoadingAssets() || InRegistry.IsGathering();
    };
    RegistryAccess.EnumerateDefinitions = [&InRegistry](TArray<FAssetData>& OutAssets)
    {
        InRegistry.GetAssetsByClass(UGamePlatformPrimaryDataAsset::StaticClass()->GetClassPathName(), OutAssets, true);
    };
    RegistryAccess.IsDefinitionAsset = [&InRegistry](const FAssetData& Asset)
    {
        return IsPlatformDefinitionAsset(InRegistry, Asset);
    };
    Startup(MoveTemp(RegistryAccess));
    BoundRegistry = &InRegistry;

    // 已核对UE5.8的CanBroadcastEvents/AssetCreated/AssetDeleted/AssetRenamed均保证GameThread。
    AssetAddedHandle = InRegistry.OnAssetAdded().AddRaw(this, &FDefinitionSourceIndex::OnAssetAddedOrUpdated);
    AssetRemovedHandle = InRegistry.OnAssetRemoved().AddRaw(this, &FDefinitionSourceIndex::OnAssetRemoved);
    AssetUpdatedHandle = InRegistry.OnAssetUpdated().AddRaw(this, &FDefinitionSourceIndex::OnAssetAddedOrUpdated);
    AssetUpdatedOnDiskHandle = InRegistry.OnAssetUpdatedOnDisk().AddRaw(this, &FDefinitionSourceIndex::OnAssetAddedOrUpdated);
    AssetRenamedHandle = InRegistry.OnAssetRenamed().AddRaw(this, &FDefinitionSourceIndex::OnAssetRenamed);
    FilesLoadedHandle = InRegistry.OnFilesLoaded().AddRaw(this, &FDefinitionSourceIndex::Invalidate);
    ScanStartedHandle = InRegistry.OnScanStarted().AddRaw(this, &FDefinitionSourceIndex::Invalidate);
    KnownGathersCompleteHandle = InRegistry.OnKnownGathersComplete().AddRaw(this, &FDefinitionSourceIndex::Invalidate);
}

void FDefinitionSourceIndex::Startup(FDefinitionSourceIndexAccess InAccess)
{
    check(IsInGameThread());
    check(InAccess.IsDiscovering && InAccess.EnumerateDefinitions && InAccess.IsDefinitionAsset);
    Shutdown();
    Access = MoveTemp(InAccess);
    Diagnostics = {};
    bActive = true;
}

void FDefinitionSourceIndex::Shutdown()
{
    check(IsInGameThread());
    bActive = false;
    ++MutationRevision; // 先取消旧快照；同步重入关闭也不能在返回后发布旧源。
    if (BoundRegistry && IAssetRegistry::Get() == BoundRegistry)
    {
        BoundRegistry->OnAssetAdded().Remove(AssetAddedHandle);
        BoundRegistry->OnAssetRemoved().Remove(AssetRemovedHandle);
        BoundRegistry->OnAssetUpdated().Remove(AssetUpdatedHandle);
        BoundRegistry->OnAssetUpdatedOnDisk().Remove(AssetUpdatedOnDiskHandle);
        BoundRegistry->OnAssetRenamed().Remove(AssetRenamedHandle);
        BoundRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
        BoundRegistry->OnScanStarted().Remove(ScanStartedHandle);
        BoundRegistry->OnKnownGathersComplete().Remove(KnownGathersCompleteHandle);
    }
    // 引擎Registry已销毁时委托也已销毁；不能在模块关闭期LoadModuleChecked重新创建它。
    BoundRegistry = nullptr;
    AssetAddedHandle.Reset();
    AssetRemovedHandle.Reset();
    AssetUpdatedHandle.Reset();
    AssetUpdatedOnDiskHandle.Reset();
    AssetRenamedHandle.Reset();
    FilesLoadedHandle.Reset();
    ScanStartedHandle.Reset();
    KnownGathersCompleteHandle.Reset();
    Access = {};
    PathsByLogicalId.Empty();
    LogicalIdsByPath.Empty();
    bBuilt = false;
}

void FDefinitionSourceIndex::Invalidate()
{
    check(IsInGameThread());
    if (!bActive) return;
    ++MutationRevision;
    ++Diagnostics.InvalidationCount;
    bBuilt = false;
    PathsByLogicalId.Empty();
    LogicalIdsByPath.Empty();
}

bool FDefinitionSourceIndex::EnsureBuilt()
{
    if (!bActive) return false;
    if (Access.IsDiscovering())
    {
        if (bBuilt) Invalidate();
        return false;
    }
    if (bBuilt) return true;
    if (bRebuilding) return false;

    // 复制访问回调，保证源枚举内同步Shutdown时不会销毁当前正在执行的回调对象。
    const FDefinitionSourceIndexAccess BuildAccess = Access;
    const uint64 BuildRevision = MutationRevision;
    TGuardValue<bool> RebuildGuard(bRebuilding, true);
    ++Diagnostics.FullScanCount;
    TArray<FAssetData> Sources;
    BuildAccess.EnumerateDefinitions(Sources);
    if (!bActive || MutationRevision != BuildRevision || BuildAccess.IsDiscovering()) return false;

    TMap<FGamePlatformId, TSet<FSoftObjectPath>> NewPathsByLogicalId;
    TMap<FSoftObjectPath, FGamePlatformId> NewLogicalIdsByPath;
    for (const FAssetData& Asset : Sources)
    {
        FGamePlatformId SourceId;
        const FSoftObjectPath SourcePath = Asset.GetSoftObjectPath();
        if (SourcePath.IsNull() || !BuildAccess.IsDefinitionAsset(Asset) || !ReadSourceIdentity(Asset, SourceId)) continue;
        // 同一物理路径只保留最后一条源记录；重复相同记录不会制造虚假冲突。
        if (const FGamePlatformId* PreviousId = NewLogicalIdsByPath.Find(SourcePath))
        {
            TSet<FSoftObjectPath>& PreviousPaths = NewPathsByLogicalId.FindChecked(*PreviousId);
            PreviousPaths.Remove(SourcePath);
            if (PreviousPaths.IsEmpty()) NewPathsByLogicalId.Remove(*PreviousId);
        }
        NewLogicalIdsByPath.Add(SourcePath, SourceId);
        NewPathsByLogicalId.FindOrAdd(SourceId).Add(SourcePath);
    }
    // 发现重新开始、标签事件或Shutdown均使整批作废；不会对外留下部分构建的索引。
    if (!bActive || MutationRevision != BuildRevision || BuildAccess.IsDiscovering()) return false;
    PathsByLogicalId = MoveTemp(NewPathsByLogicalId);
    LogicalIdsByPath = MoveTemp(NewLogicalIdsByPath);
    bBuilt = true;
    ++Diagnostics.RebuildCount;
    return true;
}

bool FDefinitionSourceIndex::CanApplyAssetEvent(const FAssetData& Asset)
{
    if (!bActive) return false;
    ++Diagnostics.AssetEventCount;
    ++MutationRevision;
    if (!bBuilt || bRebuilding || Access.IsDiscovering() || CanChangeClassHierarchy(Asset))
    {
        Invalidate();
        return false;
    }
    return true;
}

void FDefinitionSourceIndex::RemovePath(const FSoftObjectPath& Path)
{
    const FGamePlatformId* PreviousId = LogicalIdsByPath.Find(Path);
    if (!PreviousId) return;
    TSet<FSoftObjectPath>& PreviousPaths = PathsByLogicalId.FindChecked(*PreviousId);
    PreviousPaths.Remove(Path);
    if (PreviousPaths.IsEmpty()) PathsByLogicalId.Remove(*PreviousId);
    LogicalIdsByPath.Remove(Path);
}

void FDefinitionSourceIndex::AddQualifiedSource(const FAssetData& Asset)
{
    FGamePlatformId SourceId;
    const FSoftObjectPath SourcePath = Asset.GetSoftObjectPath();
    if (SourcePath.IsNull() || !ReadSourceIdentity(Asset, SourceId)) return;
    LogicalIdsByPath.Add(SourcePath, SourceId);
    PathsByLogicalId.FindOrAdd(SourceId).Add(SourcePath);
}

void FDefinitionSourceIndex::OnAssetAddedOrUpdated(const FAssetData& Asset)
{
    check(IsInGameThread());
    if (!CanApplyAssetEvent(Asset)) return;
    RemovePath(Asset.GetSoftObjectPath());
    if (Access.IsDefinitionAsset(Asset)) AddQualifiedSource(Asset);
}

void FDefinitionSourceIndex::OnAssetRemoved(const FAssetData& Asset)
{
    check(IsInGameThread());
    if (!CanApplyAssetEvent(Asset)) return;
    RemovePath(Asset.GetSoftObjectPath());
}

void FDefinitionSourceIndex::OnAssetRenamed(const FAssetData& Asset, const FString& OldObjectPath)
{
    check(IsInGameThread());
    if (!CanApplyAssetEvent(Asset)) return;
    RemovePath(FSoftObjectPath(OldObjectPath));
    RemovePath(Asset.GetSoftObjectPath());
    if (Access.IsDefinitionAsset(Asset)) AddQualifiedSource(Asset);
}

FGamePlatformResult FDefinitionSourceIndex::Resolve(const FPrimaryAssetId& DefinitionId,
    FSoftObjectPath& OutSource, const UGamePlatformPrimaryDataAsset* EditorCandidate)
{
    check(IsInGameThread());
    ++Diagnostics.QueryCount;
    OutSource.Reset();
    FGamePlatformId Parsed;
    if (DefinitionId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
        !FGamePlatformId::TryParse(DefinitionId.PrimaryAssetName.ToString(), Parsed))
        return FGamePlatformResult::Failure(TEXT("InvalidDefinitionId"), TEXT("主资产类型或规范逻辑身份非法。"));
    if (!EnsureBuilt())
        return FGamePlatformResult::Failure(TEXT("AssetRegistryNotReady"), TEXT("源资产发现尚未完成或索引已关闭，当前不能证明身份唯一。"));

    const FSoftObjectPath CandidatePath = EditorCandidate ? FSoftObjectPath(EditorCandidate) : FSoftObjectPath();
    int32 MatchCount = 0;
    FSoftObjectPath UniqueSource;
    if (const TSet<FSoftObjectPath>* RegisteredPaths = PathsByLogicalId.Find(Parsed))
    {
        for (const FSoftObjectPath& Path : *RegisteredPaths)
        {
            if (EditorCandidate && Path == CandidatePath) continue;
            ++MatchCount;
            UniqueSource = Path;
        }
    }
    // 候选无论新旧身份，都先替换自身注册路径；临时覆盖不会污染其他查询的注册快照。
    if (EditorCandidate && EditorCandidate->GetPrimaryAssetId() == DefinitionId)
    {
        ++MatchCount;
        UniqueSource = CandidatePath;
    }
    if (MatchCount == 0)
        return FGamePlatformResult::Failure(TEXT("MissingDefinition"), FString::Printf(TEXT("源注册表找不到定义：%s。"), *DefinitionId.ToString()));
    if (MatchCount != 1)
        return FGamePlatformResult::Failure(TEXT("DuplicateLogicalId"), FString::Printf(TEXT("逻辑身份%s被%d个源资产占用。"), *DefinitionId.ToString(), MatchCount));
    OutSource = UniqueSource;
    return FGamePlatformResult::Success();
}

FDefinitionSourceIndexDiagnostics FDefinitionSourceIndex::GetDiagnostics() const
{
    check(IsInGameThread());
    FDefinitionSourceIndexDiagnostics Snapshot = Diagnostics;
    Snapshot.SourceCount = LogicalIdsByPath.Num();
    Snapshot.bActive = bActive;
    Snapshot.bBuilt = bBuilt;
    return Snapshot;
}

FDefinitionSourceIndex& GetDefinitionSourceIndex()
{
    static FDefinitionSourceIndex Index;
    return Index;
}
}
