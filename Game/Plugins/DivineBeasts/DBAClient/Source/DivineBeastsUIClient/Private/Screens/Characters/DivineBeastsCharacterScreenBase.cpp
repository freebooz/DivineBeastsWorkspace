#include "Screens/Characters/DivineBeastsCharacterScreenBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

int32 UDivineBeastsCharacterScreenBase::GetPersistentCharacterCount() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().Characters.Num() : 0;
}
int32 UDivineBeastsCharacterScreenBase::GetEligibleHeroCount() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().CreateHeroOptions.Num() : 0;
}
FString UDivineBeastsCharacterScreenBase::GetSelectedPersistentCharacterId() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().SelectedCharacterId : FString();
}
