#include "Components/GamePlatformQuestTrackerWidget.h"
#include "ViewModels/GamePlatformUISnapshotScope.h"

bool UGamePlatformQuestTrackerWidget::ApplyTrackedObjectives(
    const FGamePlatformUIQuestTrackerState& InState)
{
    constexpr int32 MaxTrackedObjectives = 16;
    if (!FGamePlatformUISnapshotScope::CanAcceptNamedRevision(
            State.TrackerId, State.Revision, InState.TrackerId, InState.Revision) ||
        InState.Objectives.Num() > MaxTrackedObjectives)
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
