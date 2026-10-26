#include "Components/GamePlatformInteractionPromptWidget.h"
#include "ViewModels/GamePlatformUISnapshotScope.h"

bool UGamePlatformInteractionPromptWidget::ApplyInteractionPrompt(
    const FGamePlatformUIInteractionPromptState& InState)
{
    if (!FGamePlatformUISnapshotScope::CanAcceptNamedRevision(
            State.TargetDisplayId, State.Revision, InState.TargetDisplayId, InState.Revision))
    {
        return false;
    }
    State = InState;
    BP_OnInteractionPromptChanged(State);
    return true;
}

void UGamePlatformInteractionPromptWidget::ClearInteractionPrompt()
{
    if (State.TargetDisplayId.IsNone())
    {
        return;
    }
    State = FGamePlatformUIInteractionPromptState();
    BP_OnInteractionPromptChanged(State);
}
