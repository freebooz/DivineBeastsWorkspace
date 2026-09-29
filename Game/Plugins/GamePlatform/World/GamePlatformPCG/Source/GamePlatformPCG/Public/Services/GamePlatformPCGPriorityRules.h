#pragma once

#include "CoreMinimal.h"

/** PriorityCarve（优先级挖洞）纯规则；便于单测且不依赖World。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGPriorityRules
{
    static bool ShouldCarve(int32 SubjectPriority, int32 CarverPriority, float ExcludeMask, float Threshold)
    {
        return CarverPriority > SubjectPriority &&
               FMath::IsFinite(ExcludeMask) &&
               FMath::IsFinite(Threshold) &&
               ExcludeMask >= FMath::Clamp(Threshold, 0.0f, 1.0f);
    }
};
