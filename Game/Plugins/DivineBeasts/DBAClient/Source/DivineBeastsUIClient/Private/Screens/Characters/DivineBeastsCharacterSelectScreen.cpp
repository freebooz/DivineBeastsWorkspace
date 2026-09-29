#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"

#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"

UDivineBeastsCharacterSelectViewModel*
UDivineBeastsCharacterSelectScreen::GetCharacterSelectViewModel() const
{
    return Cast<UDivineBeastsCharacterSelectViewModel>(GetViewModel());
}
