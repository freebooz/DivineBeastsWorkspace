#include "Screens/World/DivineBeastsWorldScreenBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

FDivineBeastsUIWorldProjection UDivineBeastsWorldScreenBase::GetWorldProjection() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().World : FDivineBeastsUIWorldProjection();
}
bool UDivineBeastsWorldScreenBase::HasWorldView() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM && !VM->GetStateRef().World.WorldId.IsNone();
}
