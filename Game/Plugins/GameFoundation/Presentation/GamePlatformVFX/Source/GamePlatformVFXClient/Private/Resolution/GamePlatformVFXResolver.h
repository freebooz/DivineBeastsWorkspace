#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformVFXRequest.h"

class FGamePlatformVFXCatalogRegistry;

struct FGamePlatformVFXResolveResult
{
    bool bSuccess = false;
    bool bAmbiguous = false;
    FPrimaryAssetId DefinitionId;
    FString Error;
};

class FGamePlatformVFXResolver
{
public:
    static FGamePlatformVFXResolveResult Resolve(
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXCatalogRegistry& Registry);
};
