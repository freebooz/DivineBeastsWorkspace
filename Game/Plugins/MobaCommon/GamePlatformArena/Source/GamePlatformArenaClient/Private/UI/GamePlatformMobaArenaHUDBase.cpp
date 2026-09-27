#include "UI/GamePlatformMobaArenaHUDBase.h"

#include "ViewModels/GamePlatformArenaViewModel.h"

void UGamePlatformMobaArenaHUDBase::InitializeArenaViewModel(
    UGamePlatformArenaViewModel* InViewModel)
{
    InitializeWidgetViewModel(InViewModel);
}

UGamePlatformArenaViewModel*
UGamePlatformMobaArenaHUDBase::GetArenaViewModel() const
{
    return Cast<UGamePlatformArenaViewModel>(GetViewModel());
}
