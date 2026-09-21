#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXRegistrationHandle.h"

class UGamePlatformVFXCatalog;

class FGamePlatformVFXCatalogRegistry
{
public:
    FGamePlatformVFXRegistrationHandle Register(UGamePlatformVFXCatalog* Catalog);
    bool Unregister(const FGamePlatformVFXRegistrationHandle& Handle);
    void GetCatalogs(TArray<UGamePlatformVFXCatalog*>& OutCatalogs) const;
    void Reset();

private:
    TMap<FGuid, TWeakObjectPtr<UGamePlatformVFXCatalog>> Catalogs;
};
