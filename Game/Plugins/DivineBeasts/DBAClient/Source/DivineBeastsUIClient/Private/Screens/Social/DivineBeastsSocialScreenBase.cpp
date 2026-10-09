#include "Screens/Social/DivineBeastsSocialScreenBase.h"
#include "ViewModels/Social/DivineBeastsSocialViewModel.h"

UDivineBeastsSocialViewModel* UDivineBeastsSocialScreenBase::GetSocialViewModel() const
{
    return Cast<UDivineBeastsSocialViewModel>(GetViewModel());
}
FDivineBeastsSocialUIProjection UDivineBeastsSocialScreenBase::GetSocialProjection() const
{
    const UDivineBeastsSocialViewModel* VM = GetSocialViewModel();
    return VM ? VM->GetSnapshotRef() : FDivineBeastsSocialUIProjection();
}
