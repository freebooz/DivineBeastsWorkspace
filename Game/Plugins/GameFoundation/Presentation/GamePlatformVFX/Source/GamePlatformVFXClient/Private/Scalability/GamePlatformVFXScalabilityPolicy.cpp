#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXScalabilityPolicy::CanSpawn(EGamePlatformVFXImportance Importance, int32 ActiveInstances, int32 AmbientInstances)
{
    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    if (!Settings)
    {
        return true;
    }

    if (Importance == EGamePlatformVFXImportance::Critical)
    {
        return true;
    }

    if (ActiveInstances >= Settings->MaxActiveInstances)
    {
        return false;
    }

    if (Importance == EGamePlatformVFXImportance::Ambient && AmbientInstances >= Settings->MaxAmbientInstances)
    {
        return false;
    }

    return true;
}
