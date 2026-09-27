#include "Preloading/GamePlatformVFXPreloadCoordinator.h"
#include "Engine/StreamableManager.h"
#include "Loading/GamePlatformAssetLoader.h"

FGamePlatformVFXPreloadHandle FGamePlatformVFXPreloadCoordinator::RequestDefinition(
    const TSoftObjectPtr<UGamePlatformVFXDefinition>& Definition,
    TFunction<void(UGamePlatformVFXDefinition*)> Completion,
    FName PlatformId,
    EGamePlatformVFXQualityTier QualityTier)
{
    FGamePlatformVFXPreloadHandle Result;
    const FSoftObjectPath DefinitionPath = Definition.ToSoftObjectPath();
    if (!DefinitionPath.IsValid())
    {
        if (Completion)
        {
            Completion(nullptr);
        }
        return Result;
    }

    Result.Id = FGuid::NewGuid();
    const FGuid RequestId = Result.Id;

    TSharedPtr<FStreamableHandle> DefinitionHandle = FGamePlatformAssetLoader::RequestAsyncLoad(
        {DefinitionPath},
        FStreamableDelegate::CreateLambda([this, RequestId, Definition, Completion, PlatformId, QualityTier]() mutable
        {
            UGamePlatformVFXDefinition* LoadedDefinition = Definition.Get();
            if (!IsValid(LoadedDefinition))
            {
                Requests.Remove(RequestId);
                if (Completion)
                {
                    Completion(nullptr);
                }
                return;
            }

            if (LoadedDefinition->GetBehavior() == EGamePlatformVFXBehavior::Composite)
            {
                if (Completion)
                {
                    Completion(LoadedDefinition);
                }
                return;
            }

            const TSoftObjectPtr<UNiagaraSystem> SelectedSystem =
                LoadedDefinition->ResolveNiagaraSystem(PlatformId, QualityTier);
            const FSoftObjectPath NiagaraPath = SelectedSystem.ToSoftObjectPath();
            if (!NiagaraPath.IsValid())
            {
                Requests.Remove(RequestId);
                if (Completion)
                {
                    Completion(nullptr);
                }
                return;
            }

            const TWeakObjectPtr<UGamePlatformVFXDefinition> WeakDefinition(LoadedDefinition);
            const FSoftObjectPath LoadedDefinitionPath(LoadedDefinition);
            TArray<FSoftObjectPath> AssetsToHold;
            AssetsToHold.Add(LoadedDefinitionPath);
            AssetsToHold.Add(NiagaraPath);
            for (const TSoftObjectPtr<UObject>& Asset : LoadedDefinition->GetPreloadAssets())
            {
                const FSoftObjectPath AssetPath = Asset.ToSoftObjectPath();
                if (AssetPath.IsValid())
                {
                    AssetsToHold.AddUnique(AssetPath);
                }
            }

            TSharedPtr<FStreamableHandle> NiagaraHandle = FGamePlatformAssetLoader::RequestAsyncLoad(
                AssetsToHold,
                FStreamableDelegate::CreateLambda([this, RequestId, WeakDefinition, Completion, PlatformId, QualityTier]() mutable
                {
                    UGamePlatformVFXDefinition* FinalDefinition = WeakDefinition.Get();
                    if (Completion)
                    {
                        Completion(
                            IsValid(FinalDefinition) &&
                            FinalDefinition->ResolveNiagaraSystem(PlatformId, QualityTier).IsValid()
                                ? FinalDefinition
                                : nullptr);
                    }
                }));

            if (!NiagaraHandle.IsValid())
            {
                Requests.Remove(RequestId);
                if (Completion)
                {
                    Completion(nullptr);
                }
                return;
            }

            // 用同时持有 Definition + Niagara 的句柄替换第一阶段句柄，
            // 该句柄保留至显式 Cancel/Reset，形成真正的预加载 Lease。
            Requests.Add(RequestId, NiagaraHandle);
        }));

    if (!DefinitionHandle.IsValid())
    {
        Result.Id.Invalidate();
        if (Completion)
        {
            Completion(nullptr);
        }
        return Result;
    }

    Requests.Add(RequestId, DefinitionHandle);
    return Result;
}

bool FGamePlatformVFXPreloadCoordinator::Cancel(const FGamePlatformVFXPreloadHandle& Handle)
{
    TSharedPtr<FStreamableHandle> Streamable;
    if (!Handle.IsValid() || !Requests.RemoveAndCopyValue(Handle.Id, Streamable))
    {
        return false;
    }

    if (Streamable.IsValid())
    {
        FGamePlatformAssetLoader::Cancel(Streamable);
    }
    return true;
}

void FGamePlatformVFXPreloadCoordinator::Reset()
{
    for (TPair<FGuid, TSharedPtr<FStreamableHandle>>& Pair : Requests)
    {
        if (Pair.Value.IsValid())
        {
            FGamePlatformAssetLoader::Cancel(Pair.Value);
        }
    }
    Requests.Reset();
}
