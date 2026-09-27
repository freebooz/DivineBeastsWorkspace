#include "Services/GamePlatformInteractionFocusRules.h"

bool FGamePlatformInteractionFocusRules::SelectBestOption(
    const TArray<FGamePlatformInteractionOption>& Options,
    float Distance,
    FGamePlatformInteractionOption& OutOption)
{
    const FGamePlatformInteractionOption* Best = nullptr;

    for (const FGamePlatformInteractionOption& Option : Options)
    {
        if (!Option.bEnabled ||
            !Option.IsStructurallyValid() ||
            !FMath::IsFinite(Distance) ||
            Distance > Option.MaxDistance)
        {
            continue;
        }

        if (!Best ||
            Option.Priority > Best->Priority ||
            (Option.Priority == Best->Priority &&
             Option.MaxDistance < Best->MaxDistance) ||
            (Option.Priority == Best->Priority &&
             FMath::IsNearlyEqual(Option.MaxDistance, Best->MaxDistance) &&
             Option.OptionId.LexicalLess(Best->OptionId)))
        {
            Best = &Option;
        }
    }

    if (!Best)
    {
        return false;
    }

    OutOption = *Best;
    return true;
}
