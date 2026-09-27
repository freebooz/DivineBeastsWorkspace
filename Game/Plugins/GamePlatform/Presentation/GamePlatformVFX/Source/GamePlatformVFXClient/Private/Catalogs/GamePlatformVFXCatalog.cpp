#include "Catalogs/GamePlatformVFXCatalog.h"

FPrimaryAssetId UGamePlatformVFXCatalog::GetPrimaryAssetId() const
{
    if (StableId.IsNone())
    {
        return FPrimaryAssetId();
    }

    return FPrimaryAssetId(FPrimaryAssetType(TEXT("GamePlatformVFXCatalog")), StableId);
}
