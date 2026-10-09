#include "Screens/Account/DivineBeastsAccountScreenBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

const FDivineBeastsUIViewState* UDivineBeastsAccountScreenBase::GetAccountState() const
{
    const UDivineBeastsUIViewModel* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? &VM->GetStateRef() : nullptr;
}
bool UDivineBeastsAccountScreenBase::IsAccountAuthenticated() const
{
    const FDivineBeastsUIViewState* State = GetAccountState();
    return State && State->bAuthenticated;
}
bool UDivineBeastsAccountScreenBase::IsAccountActionBusy() const
{
    const FDivineBeastsUIViewState* State = GetAccountState();
    return !State || State->bBusy || State->bMaintenance;
}
FText UDivineBeastsAccountScreenBase::GetAccountErrorText() const
{
    const FDivineBeastsUIViewState* State = GetAccountState();
    return State ? State->ErrorText : FText::GetEmpty();
}
