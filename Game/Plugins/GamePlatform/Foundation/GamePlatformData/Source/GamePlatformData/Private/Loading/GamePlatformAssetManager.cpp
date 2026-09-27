#include "Loading/GamePlatformAssetManager.h"
#include "Loading/DataNextTick.h"
#include "Ownership/DataDemandLedger.h"
#include "Engine/StreamableManager.h"
#include "Validation/GamePlatformDefinitionValidation.h"

namespace
{
// FName按大小写不敏感比较，而std::string不是；键必须规范化，避免同一分组/主资产被账本拆成两份。
std::string LedgerKey(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value.ToLower())); }
struct FAssetDemand
{
    // 首次接管前的引擎状态；不把已有加载当作本插件可无条件卸载的所有权。
    bool bHasExternalBaseline = false;
    TArray<FName> BaselineBundles;
    TSharedPtr<FStreamableHandle> BaselinePendingHandle;
    TSharedPtr<FStreamableHandle> BaselineCurrentHandle;
    TSharedPtr<FStreamableHandle> Operation;
    TMap<FString, TFunction<void(FGamePlatformResult)>> Waiters;
    FGuid Serial;
};
}
struct FGamePlatformProcessDemands
{
    GamePlatform::Data::FDemandLedger Ledger;
    TMap<FPrimaryAssetId, FAssetDemand> Assets;
    bool bIsApplyingOwnedDemand = false;
};

UGamePlatformAssetManager::UGamePlatformAssetManager() : ProcessDemands(MakeUnique<FGamePlatformProcessDemands>()) {}
UGamePlatformAssetManager::UGamePlatformAssetManager(FVTableHelper& Helper) : Super(Helper), ProcessDemands(MakeUnique<FGamePlatformProcessDemands>()) {}
UGamePlatformAssetManager::~UGamePlatformAssetManager() = default;

TSharedPtr<FStreamableHandle> UGamePlatformAssetManager::ChangeBundleStateForPrimaryAssets(
    const TArray<FPrimaryAssetId>& AssetsToChange, const TArray<FName>& AddBundles,
    const TArray<FName>& RemoveBundles, bool bRemoveAllBundles, FAssetManagerLoadParams&& LoadParams,
    UE::FSourceLocation Location)
{
    check(IsInGameThread());
    if (ProcessDemands->bIsApplyingOwnedDemand ||
        !AssetsToChange.ContainsByPredicate([this](const FPrimaryAssetId& Id) { return ProcessDemands->Assets.Contains(Id); }))
        return Super::ChangeBundleStateForPrimaryAssets(AssetsToChange, AddBundles, RemoveBundles, bRemoveAllBundles, MoveTemp(LoadParams), MoveTemp(Location));

    // 不能只比对句柄：同分组LoadPrimaryAsset可能没有新句柄。此引擎入口实际捕捉每次外部请求。
    // 各资产的有效集合不同，逐资产调用原生AssetManager，再仅用引擎组合完成句柄；不另行加载资源。
    TArray<TSharedPtr<FStreamableHandle>> Handles;
    for (const FPrimaryAssetId& Id : AssetsToChange)
    {
        TArray<FName> Desired;
        FAssetDemand* Entry = ProcessDemands->Assets.Find(Id);
        if (Entry && !ProcessDemands->Ledger.HasDemand(LedgerKey(Id.ToString())))
        {
            // 最后租约已释放，新的显式外部请求重新接管引擎状态；撤销尚未执行的基线恢复回调。
            ProcessDemands->Assets.Remove(Id);
            Entry = nullptr;
        }
        if (Entry)
        {
            Entry->Serial = FGuid::NewGuid(); // 在外部操作可能取消旧句柄前，让旧完成失效。
            if (!bRemoveAllBundles) GetPrimaryAssetHandle(Id, false, &Desired);
            for (FName Bundle : RemoveBundles) Desired.Remove(Bundle);
            for (FName Bundle : AddBundles) Desired.AddUnique(Bundle);
            Entry->bHasExternalBaseline = true;
            for (FName Bundle : Desired) Entry->BaselineBundles.AddUnique(Bundle);
            Desired = Entry->BaselineBundles;
            for (const auto& Bundle : ProcessDemands->Ledger.Bundles(LedgerKey(Id.ToString())))
                Desired.AddUnique(FName(UTF8_TO_TCHAR(Bundle.c_str())));
            Desired.Sort(FNameLexicalLess());
        }
        FAssetManagerLoadParams PerAsset;
        PerAsset.Priority = LoadParams.Priority;
        PerAsset.DownloadParams = LoadParams.DownloadParams;
        auto Handle = Super::ChangeBundleStateForPrimaryAssets({Id}, Entry ? Desired : AddBundles,
            Entry ? TArray<FName>() : RemoveBundles, Entry ? true : bRemoveAllBundles, MoveTemp(PerAsset), Location);
        if (Handle) Handles.Add(Handle);
        if (Entry)
        {
            // 外部待加载句柄作为新基线，后续内部Reconcile先等待它，不能取消它来收缩租约集合。
            Entry = ProcessDemands->Assets.Find(Id);
            if (Entry)
            {
                Entry->BaselinePendingHandle = GetPrimaryAssetHandle(Id, false);
                Entry->BaselineCurrentHandle = GetPrimaryAssetHandle(Id, true);
                const FGuid Serial = Entry->Serial;
                TWeakObjectPtr<UGamePlatformAssetManager> WeakThis(this);
                GamePlatform::Data::NextTick([WeakThis, Id, Serial]()
                {
                    if (auto* Self = WeakThis.Get())
                        if (const auto* Current = Self->ProcessDemands->Assets.Find(Id); Current && Current->Serial == Serial)
                            Self->Reconcile(Id);
                });
            }
        }
    }
    FStreamableAsyncLoadParams CompletionParams;
    CompletionParams.OnComplete = MoveTemp(LoadParams.OnComplete);
    CompletionParams.OnCancel = MoveTemp(LoadParams.OnCancel);
    CompletionParams.OnUpdate = MoveTemp(LoadParams.OnUpdate);
    return StreamableManager.CreateCombinedHandle(Handles, TEXT("GamePlatformExternalPrimaryAssets"),
        EStreamableManagerCombinedHandleOptions::None, MoveTemp(CompletionParams), MoveTemp(Location));
}

int32 UGamePlatformAssetManager::UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload)
{
    check(IsInGameThread());
    TArray<FPrimaryAssetId> Allowed;
    for (const FPrimaryAssetId& Id : AssetsToUnload)
        if (!ProcessDemands->Ledger.HasDemand(LedgerKey(Id.ToString())))
        {
            ProcessDemands->Assets.Remove(Id); // 外部明确卸载不应被迟到的基线恢复回调重新加载。
            Allowed.Add(Id);
        }
    return Super::UnloadPrimaryAssets(Allowed);
}

FGamePlatformResult UGamePlatformAssetManager::AddDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey,
    const TArray<FName>& Bundles, TFunction<void(FGamePlatformResult)> Completion)
{
    check(IsInGameThread());
    FSoftObjectPath Source;
    FGamePlatformResult SourceResult = ResolveUniqueGamePlatformDefinitionSource(AssetId, Source);
    if (!SourceResult.IsSuccess()) return SourceResult;
    if (GetPrimaryAssetPath(AssetId).IsNull())
        return FGamePlatformResult::Failure(TEXT("MissingDefinition"), FString::Printf(TEXT("未发现主资产：%s。"), *AssetId.ToString()));
    if (GetPrimaryAssetPath(AssetId) != Source)
        return FGamePlatformResult::Failure(TEXT("PrimaryAssetMappingMismatch"), TEXT("主资产映射与唯一源路径不同，须修复扫描配置或重新保存资产。"));
    GamePlatform::Data::FDemandLedger::FBundleSet Requested;
    for (FName Bundle : Bundles) Requested.insert(LedgerKey(Bundle.ToString()));
    if (!ProcessDemands->Ledger.Add(LedgerKey(AssetId.ToString()), LedgerKey(LeaseKey), Requested))
        return FGamePlatformResult::Failure(TEXT("DuplicateDemand"), TEXT("同一资产的同一租约已登记，不覆盖既有需求。"));
    if (!ProcessDemands->Assets.Contains(AssetId))
    {
        FAssetDemand Initial;
        Initial.BaselinePendingHandle = GetPrimaryAssetHandle(AssetId, false, &Initial.BaselineBundles);
        TArray<FName> CurrentBundles;
        Initial.BaselineCurrentHandle = GetPrimaryAssetHandle(AssetId, true, &CurrentBundles);
        for (FName Bundle : CurrentBundles) Initial.BaselineBundles.AddUnique(Bundle);
        Initial.bHasExternalBaseline = Initial.BaselinePendingHandle.IsValid() || Initial.BaselineCurrentHandle.IsValid() || GetPrimaryAssetObject(AssetId) != nullptr;
        ProcessDemands->Assets.Add(AssetId, MoveTemp(Initial));
    }
    ProcessDemands->Assets.FindChecked(AssetId).Waiters.Add(LeaseKey, MoveTemp(Completion));
    Reconcile(AssetId);
    return FGamePlatformResult::Success();
}

void UGamePlatformAssetManager::RemoveDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey)
{
    check(IsInGameThread());
    if (!ProcessDemands->Ledger.Remove(LedgerKey(AssetId.ToString()), LedgerKey(LeaseKey))) return;
    if (FAssetDemand* Entry = ProcessDemands->Assets.Find(AssetId)) Entry->Waiters.Remove(LeaseKey);
    Reconcile(AssetId);
}

void UGamePlatformAssetManager::Reconcile(const FPrimaryAssetId& AssetId)
{
    FAssetDemand* Entry = ProcessDemands->Assets.Find(AssetId);
    if (!Entry) return;
    Entry->Serial = FGuid::NewGuid();
    const FGuid Serial = Entry->Serial;
    TWeakObjectPtr<UGamePlatformAssetManager> WeakThis(this);
    // 外部原有异步加载尚未完成时先等它完成，不能为了改变分组取消外部请求。
    if (Entry->BaselinePendingHandle && !Entry->BaselinePendingHandle->HasLoadCompleted() && !Entry->BaselinePendingHandle->WasCanceled())
    {
        GamePlatform::Data::NextTick([WeakThis, AssetId, Serial]()
        {
            if (auto* Self = WeakThis.Get())
                if (auto* Current = Self->ProcessDemands->Assets.Find(AssetId); Current && Current->Serial == Serial) Self->Reconcile(AssetId);
        });
        return;
    }
    const bool bHasDemand = ProcessDemands->Ledger.HasDemand(LedgerKey(AssetId.ToString()));
    if (!bHasDemand && !Entry->bHasExternalBaseline)
    {
        // 先移除代次与回调，再让引擎取消句柄；同步取消不得复活旧请求。
        ProcessDemands->Assets.Remove(AssetId);
        UnloadPrimaryAsset(AssetId);
        return;
    }
    TArray<FName> Desired = Entry->BaselineBundles;
    for (const auto& Bundle : ProcessDemands->Ledger.Bundles(LedgerKey(AssetId.ToString())))
        Desired.AddUnique(FName(UTF8_TO_TCHAR(Bundle.c_str())));
    Desired.Sort(FNameLexicalLess());
    FStreamableDelegate Complete = FStreamableDelegate::CreateLambda([WeakThis, AssetId, Serial]()
    {
        GamePlatform::Data::NextTick([WeakThis, AssetId, Serial]()
        {
            if (auto* Self = WeakThis.Get()) Self->CompleteReconcile(AssetId, Serial);
        });
    });
    // LoadPrimaryAsset会以给定集合替换分组；传入所有调用者并集及外部基线，而非最后一次请求。
    FAssetManagerLoadParams LoadParams;
    LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateLambda([Complete](TSharedPtr<FStreamableHandle>) { Complete.ExecuteIfBound(); });
    LoadParams.OnCancel = LoadParams.OnComplete;
    TSharedPtr<FStreamableHandle> Operation;
    {
        TGuardValue<bool> OwnedCall(ProcessDemands->bIsApplyingOwnedDemand, true);
        Operation = LoadPrimaryAsset(AssetId, Desired, MoveTemp(LoadParams));
    }
    // 引擎可同步通知；用户通知已延后，重新查询可防未来重入改动悬空Entry。
    if (FAssetDemand* Current = ProcessDemands->Assets.Find(AssetId); Current && Current->Serial == Serial)
        Current->Operation = Operation;
    // UE5.8无新工作允许空句柄；走相同校验路径，重复完成由Serial及取走Waiters去重。
    if (!Operation || Operation->HasLoadCompleted() || Operation->WasCanceled()) Complete.ExecuteIfBound();
}

void UGamePlatformAssetManager::CompleteReconcile(FPrimaryAssetId AssetId, FGuid Serial)
{
    check(IsInGameThread());
    FAssetDemand* Entry = ProcessDemands->Assets.Find(AssetId);
    if (!Entry || Entry->Serial != Serial) return;
    if (Entry->Operation && !Entry->Operation->HasLoadCompleted() && !Entry->Operation->WasCanceled()) return;
    const bool bFailed = (Entry->Operation && (Entry->Operation->WasCanceled() || Entry->Operation->HasError())) || !GetPrimaryAssetObject(AssetId);
    FGamePlatformResult Result = bFailed
        ? FGamePlatformResult::Failure(TEXT("AssetLoadFailed"), FString::Printf(TEXT("引擎未能完成定义或分组资源加载：%s。"), *AssetId.ToString()))
        : FGamePlatformResult::Success();
    auto Waiters = MoveTemp(Entry->Waiters);
    Entry->Waiters.Empty();
    if (!ProcessDemands->Ledger.HasDemand(LedgerKey(AssetId.ToString())))
        ProcessDemands->Assets.Remove(AssetId); // 外部基线仍由引擎持有，绝不Unload。
    for (auto& Waiter : Waiters) Waiter.Value(Result);
}
