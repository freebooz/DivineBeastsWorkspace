#include "Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h"
#include "ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h"

UDivineBeastsLiveOpsViewModel* UDivineBeastsLiveOpsScreenBase::GetLiveOpsViewModel() const
{
    return Cast<UDivineBeastsLiveOpsViewModel>(GetViewModel());
}
FDivineBeastsLiveOpsUIProjection UDivineBeastsLiveOpsScreenBase::GetLiveOpsProjection() const
{
    const UDivineBeastsLiveOpsViewModel* VM = GetLiveOpsViewModel();
    return VM ? VM->GetSnapshotRef() : FDivineBeastsLiveOpsUIProjection();
}
