#include "UI/GamePlatformMobaArenaModalScreenBase.h"

#include "ViewModels/GamePlatformArenaViewModel.h"

UGamePlatformArenaViewModel*
UGamePlatformMobaArenaModalScreenBase::GetArenaViewModel() const
{
    return Cast<UGamePlatformArenaViewModel>(GetViewModel());
}
