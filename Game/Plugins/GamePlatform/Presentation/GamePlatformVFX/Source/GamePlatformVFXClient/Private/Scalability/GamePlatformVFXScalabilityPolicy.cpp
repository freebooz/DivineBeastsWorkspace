#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
    const FGamePlatformVFXRequest& Request,
    int32 ActiveInstances,
    const UGamePlatformVFXSettings& Settings)
{
    // HardMaxTrackedInstances是绝对安全上限，任何重要度都不能绕过。
    if (ActiveInstances >= FMath::Max(1, Settings.HardMaxTrackedInstances))
    {
        return false;
    }

    if (Request.Importance == EGamePlatformVFXImportance::Critical)
    {
        return true;
    }

    return ActiveInstances < FMath::Max(1, Settings.MaxActiveInstances);
}
