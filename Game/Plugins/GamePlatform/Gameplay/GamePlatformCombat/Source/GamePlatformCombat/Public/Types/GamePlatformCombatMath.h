#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformCombatResult.h"

class GAMEPLATFORMCOMBAT_API FGamePlatformCombatMath
{
public:
    static FGamePlatformCombatResult ResolveDamage(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float CurrentShield,
        float CurrentHealth,
        bool bBypassShield);

    static FGamePlatformCombatResult ResolveHealing(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float CurrentHealth,
        float MaxHealth,
        float CurrentShield);
};
