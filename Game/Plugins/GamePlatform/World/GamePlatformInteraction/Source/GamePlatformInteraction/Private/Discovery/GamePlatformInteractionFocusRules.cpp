#include "Services/GamePlatformInteractionFocusRules.h"

bool FGamePlatformInteractionFocusRules::IsPreferredOption(
    const FGamePlatformInteractionOption& Candidate,
    const FGamePlatformInteractionOption* CurrentBest)
{
    return !CurrentBest ||
           Candidate.Priority > CurrentBest->Priority ||
           (Candidate.Priority == CurrentBest->Priority &&
            Candidate.MaxDistance < CurrentBest->MaxDistance) ||
           (Candidate.Priority == CurrentBest->Priority &&
            FMath::IsNearlyEqual(
                Candidate.MaxDistance,
                CurrentBest->MaxDistance) &&
            Candidate.OptionId.LexicalLess(CurrentBest->OptionId));
}

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

        if (IsPreferredOption(Option, Best))
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
