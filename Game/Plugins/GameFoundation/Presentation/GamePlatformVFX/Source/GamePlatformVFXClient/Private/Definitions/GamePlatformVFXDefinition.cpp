#include "Definitions/GamePlatformVFXDefinition.h"

FPrimaryAssetId UGamePlatformVFXDefinition::GetPrimaryAssetId() const
{
    if (StableId.IsNone())
    {
        return FPrimaryAssetId();
    }

    return FPrimaryAssetId(FPrimaryAssetType(TEXT("GamePlatformVFXDefinition")), StableId);
}
