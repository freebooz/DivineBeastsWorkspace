#include "Screens/DivineBeastsUIScreen.h"

#include "ViewModels/DivineBeastsUIViewModel.h"

void UDivineBeastsUIScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenActivated();
    }
}

void UDivineBeastsUIScreen::NativeOnDeactivated()
{
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenDeactivated();
    }
    Super::NativeOnDeactivated();
}
