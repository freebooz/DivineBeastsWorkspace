#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionOption.h"

class GAMEPLATFORMINTERACTION_API FGamePlatformInteractionFocusRules
{
public:
    static bool SelectBestOption(
        const TArray<FGamePlatformInteractionOption>& Options,
        float Distance,
        FGamePlatformInteractionOption& OutOption);
};
