#include "Dialogs/GamePlatformDialogWidget.h"

void UGamePlatformDialogWidget::NativeOnActivated()
{
    bResolved = false;
    Super::NativeOnActivated();
}

bool UGamePlatformDialogWidget::Resolve(FName ResultId)
{
    if (bResolved)
    {
        return false;
    }

    bResolved = true;
    OnResolved.Broadcast(ResultId);
    CloseScreen();
    return true;
}
