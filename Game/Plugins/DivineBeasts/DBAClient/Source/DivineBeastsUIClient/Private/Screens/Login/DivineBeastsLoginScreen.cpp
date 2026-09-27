#include "Screens/Login/DivineBeastsLoginScreen.h"

#include "ViewModels/Login/DivineBeastsLoginViewModel.h"

UDivineBeastsLoginViewModel*
UDivineBeastsLoginScreen::GetLoginViewModel() const
{
    // 页面不缓存第二份 ViewModel 指针，避免状态所有权重复。
    return Cast<UDivineBeastsLoginViewModel>(GetViewModel());
}
