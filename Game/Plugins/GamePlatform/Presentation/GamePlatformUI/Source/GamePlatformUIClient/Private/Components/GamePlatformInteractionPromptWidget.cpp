#include "Components/GamePlatformInteractionPromptWidget.h"

bool UGamePlatformInteractionPromptWidget::ApplyInteractionPrompt(
    const FGamePlatformUIInteractionPromptState& InState)
{
    if (InState.TargetDisplayId.IsNone() || InState.Revision < 0 ||
        (State.TargetDisplayId == InState.TargetDisplayId &&
         InState.Revision <= State.Revision))
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
