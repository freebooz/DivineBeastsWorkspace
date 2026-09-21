#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Types/GamePlatformVFXPreloadHandle.h"
#include "UObject/PrimaryAssetId.h"

class FGamePlatformVFXPreloadCoordinator
{
public:
    FGamePlatformVFXPreloadHandle Preload(const FPrimaryAssetId& DefinitionId);
    bool Release(const FGamePlatformVFXPreloadHandle& Handle);
    void Reset();

private:
    struct FLease
    {
        FPrimaryAssetId DefinitionId;
        TSharedPtr<FStreamableHandle> DefinitionHandle;
        TSharedPtr<FStreamableHandle> NiagaraHandle;
    };

    TMap<FGuid, FLease> Leases;
    void OnDefinitionLoaded(FGuid LeaseId);
};
