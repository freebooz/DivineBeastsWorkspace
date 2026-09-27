#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
    const FGamePlatformVFXRequest& Request,
    int32 ActiveInstances,
    const UGamePlatformVFXSettings& Settings)
{
    if (Request.Importance == EGamePlatformVFXImportance::Critical)
    {
        return true;
    }

    return ActiveInstances < FMath::Max(1, Settings.MaxActiveInstances);
}
