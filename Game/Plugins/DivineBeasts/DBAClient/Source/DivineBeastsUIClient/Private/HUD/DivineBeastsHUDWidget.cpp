#include "HUD/DivineBeastsHUDWidget.h"

#include "ViewModels/DivineBeastsUIViewModel.h"

void UDivineBeastsHUDWidget::BindUIEvents()
{
    Super::BindUIEvents();

    // 复用当前项目通用 ViewModel 的事件订阅入口。
    // 后续领域 HUD ViewModel 仍可继承该类型而无需改变 HUD 生命周期代码。
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenActivated();
    }
}

void UDivineBeastsHUDWidget::UnbindUIEvents()
{
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenDeactivated();
    }

    Super::UnbindUIEvents();
}
