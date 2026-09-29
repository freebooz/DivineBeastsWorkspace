#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"

#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"

UDivineBeastsCharacterCreateViewModel*
UDivineBeastsCharacterCreateScreen::GetCharacterCreateViewModel() const
{
    return Cast<UDivineBeastsCharacterCreateViewModel>(GetViewModel());
}
