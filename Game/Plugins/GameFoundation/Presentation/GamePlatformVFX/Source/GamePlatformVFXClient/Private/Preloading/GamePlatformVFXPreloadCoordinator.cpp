#include "Preloading/GamePlatformVFXPreloadCoordinator.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Engine/AssetManager.h"

FGamePlatformVFXPreloadHandle FGamePlatformVFXPreloadCoordinator::Preload(const FPrimaryAssetId& DefinitionId)
{
    FGamePlatformVFXPreloadHandle PublicHandle;
    if (!DefinitionId.IsValid())
    {
        return PublicHandle;
    }

    UAssetManager& AssetManager = UAssetManager::Get();
    const FSoftObjectPath DefinitionPath = AssetManager.GetPrimaryAssetPath(DefinitionId);
    if (!DefinitionPath.IsValid())
    {
        return PublicHandle;
    }

    PublicHandle.RequestId = FGuid::NewGuid();
    FLease& Lease = Leases.Add(PublicHandle.RequestId);
    Lease.DefinitionId = DefinitionId;
    Lease.DefinitionHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        DefinitionPath,
        FStreamableDelegate::CreateRaw(this, &FGamePlatformVFXPreloadCoordinator::OnDefinitionLoaded, PublicHandle.RequestId));
    return PublicHandle;
}

void FGamePlatformVFXPreloadCoordinator::OnDefinitionLoaded(FGuid LeaseId)
{
    FLease* Lease = Leases.Find(LeaseId);
    if (!Lease)
    {
        return;
    }

    UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(Lease->DefinitionId);
    if (!LoadedObject)
    {
        const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Lease->DefinitionId);
        LoadedObject = Path.ResolveObject();
    }

    const UGamePlatformVFXDefinition* Definition = Cast<UGamePlatformVFXDefinition>(LoadedObject);
    if (!Definition || Definition->NiagaraSystem.IsNull())
    {
        return;
    }

    Lease->NiagaraHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Definition->NiagaraSystem.ToSoftObjectPath(), FStreamableDelegate());
}

bool FGamePlatformVFXPreloadCoordinator::Release(const FGamePlatformVFXPreloadHandle& Handle)
{
    FLease* Lease = Leases.Find(Handle.RequestId);
    if (!Lease)
    {
        return false;
    }

    if (Lease->NiagaraHandle) { Lease->NiagaraHandle->ReleaseHandle(); }
    if (Lease->DefinitionHandle) { Lease->DefinitionHandle->ReleaseHandle(); }
    Leases.Remove(Handle.RequestId);
    return true;
}

void FGamePlatformVFXPreloadCoordinator::Reset()
{
    TArray<FGuid> Ids;
    Leases.GetKeys(Ids);
    for (const FGuid& Id : Ids)
    {
        FGamePlatformVFXPreloadHandle Handle;
        Handle.RequestId = Id;
        Release(Handle);
    }
}
