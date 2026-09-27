#include "Facts/DivineBeastsPresentationFacts.h"

bool FDivineBeastsVillageFeedbackPresentationFact::IsValid() const
{
    return FactId.IsValid() &&
           !FeedbackId.IsNone() &&
           (ExperienceId == TEXT("Experience.Village.Tutorial") ||
            ExperienceId == TEXT("Experience.Village.Training"));
}
