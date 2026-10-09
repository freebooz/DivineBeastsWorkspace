#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

FDivineBeastsUIWorldProjection UDivineBeastsWorldHUDBase::GetWorldProjection() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().World : FDivineBeastsUIWorldProjection();
}
bool UDivineBeastsWorldHUDBase::HasWorldView() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM && !VM->GetStateRef().World.WorldId.IsNone();
}
