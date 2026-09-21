#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Catalogs/GamePlatformVFXCatalog.h"

FGamePlatformVFXRegistrationHandle FGamePlatformVFXCatalogRegistry::Register(UGamePlatformVFXCatalog* Catalog)
{
    FGamePlatformVFXRegistrationHandle Handle;
    if (!IsValid(Catalog))
    {
        return Handle;
    }

    Handle.RegistrationId = FGuid::NewGuid();
    Catalogs.Add(Handle.RegistrationId, Catalog);
    return Handle;
}

bool FGamePlatformVFXCatalogRegistry::Unregister(const FGamePlatformVFXRegistrationHandle& Handle)
{
    return Handle.IsValid() && Catalogs.Remove(Handle.RegistrationId) > 0;
}

void FGamePlatformVFXCatalogRegistry::GetCatalogs(TArray<UGamePlatformVFXCatalog*>& OutCatalogs) const
{
    OutCatalogs.Reset();
    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformVFXCatalog>>& Pair : Catalogs)
    {
        if (UGamePlatformVFXCatalog* Catalog = Pair.Value.Get())
        {
            OutCatalogs.Add(Catalog);
        }
    }
}

void FGamePlatformVFXCatalogRegistry::Reset()
{
    Catalogs.Reset();
}
