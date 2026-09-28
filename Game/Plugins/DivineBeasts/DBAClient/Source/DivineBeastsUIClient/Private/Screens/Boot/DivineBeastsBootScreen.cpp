#include "Screens/Boot/DivineBeastsBootScreen.h"

#include "ViewModels/Boot/DivineBeastsBootViewModel.h"

UDivineBeastsBootViewModel*
UDivineBeastsBootScreen::GetBootViewModel() const
{
    // 页面只读取平台 Screen 已持有的唯一 ViewModel，避免再维护第二份状态所有权。
    return Cast<UDivineBeastsBootViewModel>(GetViewModel());
}
