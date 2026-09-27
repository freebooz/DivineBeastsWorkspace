#pragma once

#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Types/GamePlatformVFXRegistrationHandle.h"
#include "Types/GamePlatformVFXRequest.h"

struct FGamePlatformVFXResolvedDefinition
{
    TSoftObjectPtr<UGamePlatformVFXDefinition> Definition;
    FName DefinitionId = NAME_None;
    bool bAmbiguous = false;
    uint64 RegistryRevision = 0;

    bool IsValid() const { return !bAmbiguous && !Definition.IsNull(); }
};

/** 已加载 Catalog 的世界级注册表。 */
class FGamePlatformVFXCatalogRegistry
{
public:
    FGamePlatformVFXRegistrationHandle Register(UGamePlatformVFXCatalog* Catalog);
    bool Unregister(const FGamePlatformVFXRegistrationHandle& Handle);
    FGamePlatformVFXResolvedDefinition Resolve(const FGamePlatformVFXRequest& Request) const;
    void Reset();
    int32 Num() const { return Catalogs.Num(); }
    uint64 GetRevision() const { return Revision; }

private:
    struct FRegisteredCatalog
    {
        FGamePlatformVFXRegistrationHandle Handle;
        TWeakObjectPtr<UGamePlatformVFXCatalog> Catalog;
    };

    FString MakeCacheKey(const FGamePlatformVFXRequest& Request) const;
    void InvalidateCache();

    TArray<FRegisteredCatalog> Catalogs;
    mutable TMap<FString, FGamePlatformVFXResolvedDefinition> ResolveCache;
    uint64 Revision = 1;
};
