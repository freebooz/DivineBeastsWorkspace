#pragma once

#include "CoreMinimal.h"
#include "GamePlatformAITargetSelectionRules.generated.h"

USTRUCT()
struct GAMEPLATFORMAI_API FGamePlatformAITargetSortKey
{
    GENERATED_BODY()

    bool bEligible = false;
    bool bVisible = false;
    float DistanceSquared = TNumericLimits<float>::Max();
    double LastSensedTime = 0.0;
    FGuid EntityId;
};

class GAMEPLATFORMAI_API FGamePlatformAITargetSelectionRules
{
public:
    static bool IsBetter(
        const FGamePlatformAITargetSortKey& Candidate,
        const FGamePlatformAITargetSortKey& Current,
        bool bPreferVisible);
};
