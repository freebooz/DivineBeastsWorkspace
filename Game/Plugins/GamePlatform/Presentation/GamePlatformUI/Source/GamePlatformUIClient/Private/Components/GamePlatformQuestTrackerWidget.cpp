#include "Components/GamePlatformQuestTrackerWidget.h"

bool UGamePlatformQuestTrackerWidget::ApplyTrackedObjectives(
    const FGamePlatformUIQuestTrackerState& InState)
{
    constexpr int32 MaxTrackedObjectives = 16;
    if (InState.TrackerId.IsNone() || InState.Revision < 0 ||
        InState.Objectives.Num() > MaxTrackedObjectives ||
        (State.TrackerId == InState.TrackerId &&
         InState.Revision <= State.Revision))
    {
        return false;
    }
    TSet<FName> UsedIds;
    for (const FGamePlatformUITrackedObjective& Item : InState.Objectives)
    {
        if (Item.ObjectiveId.IsNone() || UsedIds.Contains(Item.ObjectiveId) ||
            !FMath::IsFinite(Item.Progress))
        {
            return false;
        }
        UsedIds.Add(Item.ObjectiveId);
    }

    State = InState;
    for (FGamePlatformUITrackedObjective& Item : State.Objectives)
    {
        Item.Progress = FMath::Clamp(Item.Progress, 0.0f, 1.0f);
    }
    BP_OnObjectivesChanged(State);
    return true;
}
