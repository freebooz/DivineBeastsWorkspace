#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"

#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"

UDivineBeastsLoadingViewModel*
UDivineBeastsLoadingTravelScreen::GetLoadingViewModel() const
{
    // 使用平台 Screen 已持有的唯一 ViewModel，不复制加载状态所有权。
    return Cast<UDivineBeastsLoadingViewModel>(GetViewModel());
}
