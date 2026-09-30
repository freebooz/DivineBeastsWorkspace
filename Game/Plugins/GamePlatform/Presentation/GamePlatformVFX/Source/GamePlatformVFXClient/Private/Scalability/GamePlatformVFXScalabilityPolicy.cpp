#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
    const FGamePlatformVFXRequest& Request,
    int32 ActiveInstances,
    const UGamePlatformVFXSettings& Settings)
{
    const int32 HardLimit = FMath::Max(1, Settings.HardMaxTrackedInstances);
    if (ActiveInstances >= HardLimit)
    {
        return false;
    }

    if (Request.Importance == EGamePlatformVFXImportance::Critical)
    {
        return true;
    }

    const int32 CombatLimit = FMath::Clamp(
        Settings.MaxActiveInstances,
        1,
        HardLimit);
    const int32 StatusLimit = FMath::Clamp(
        Settings.MaxStatusInstances,
        1,
        CombatLimit);
    const int32 AmbientLimit = FMath::Clamp(
        Settings.MaxAmbientInstances,
        1,
        StatusLimit);

    switch (Request.Importance)
    {
    case EGamePlatformVFXImportance::Ambient:
        return ActiveInstances < AmbientLimit;
    case EGamePlatformVFXImportance::Status:
        return ActiveInstances < StatusLimit;
    case EGamePlatformVFXImportance::Combat:
    default:
        return ActiveInstances < CombatLimit;
    }
}
