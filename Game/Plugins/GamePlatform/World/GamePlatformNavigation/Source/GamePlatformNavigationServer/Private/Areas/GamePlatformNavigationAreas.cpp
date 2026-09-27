#include "Areas/GamePlatformNavigationAreas.h"
#include "Settings/GamePlatformNavigationSettings.h"

UGamePlatformNavArea_Default::UGamePlatformNavArea_Default()
{
    DefaultCost = 1.0f;
    FixedAreaEnteringCost = 0.0f;
}

UGamePlatformNavArea_HighCost::UGamePlatformNavArea_HighCost()
{
    DefaultCost =
        FMath::Max(
            1.0f,
            GetDefault<UGamePlatformNavigationSettings>()
                ->HighCostAreaMultiplier);
    FixedAreaEnteringCost = 0.0f;
}
