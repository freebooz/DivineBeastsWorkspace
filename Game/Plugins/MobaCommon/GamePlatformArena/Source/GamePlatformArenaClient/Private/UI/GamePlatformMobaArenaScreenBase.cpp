#include "UI/GamePlatformMobaArenaScreenBase.h"

#include "ViewModels/GamePlatformArenaViewModel.h"

UGamePlatformArenaViewModel*
UGamePlatformMobaArenaScreenBase::GetArenaViewModel() const
{
    return Cast<UGamePlatformArenaViewModel>(GetViewModel());
}
