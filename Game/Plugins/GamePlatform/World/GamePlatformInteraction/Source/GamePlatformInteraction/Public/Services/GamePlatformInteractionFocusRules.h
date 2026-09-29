#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionOption.h"

class GAMEPLATFORMINTERACTION_API FGamePlatformInteractionFocusRules
{
public:
    /** 比较两个 Option（交互选项）的稳定优先级；用于单目标和多组件候选统一排序。 */
    static bool IsPreferredOption(
        const FGamePlatformInteractionOption& Candidate,
        const FGamePlatformInteractionOption* CurrentBest);

    static bool SelectBestOption(
        const TArray<FGamePlatformInteractionOption>& Options,
        float Distance,
        FGamePlatformInteractionOption& OutOption);
};
