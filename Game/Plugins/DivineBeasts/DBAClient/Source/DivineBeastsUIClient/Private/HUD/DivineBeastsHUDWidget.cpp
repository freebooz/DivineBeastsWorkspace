#include "HUD/DivineBeastsHUDWidget.h"

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "UObject/StrongObjectPtr.h"

void UDivineBeastsHUDWidget::BindUIEvents()
{
    Super::BindUIEvents();

    bHUDEventsBound = true;
    ReconcileHUDModel();
}

void UDivineBeastsHUDWidget::UnbindUIEvents()
{
    bHUDEventsBound = false; ++HUDBindingGeneration;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundHUDModel.Get());
    BoundHUDModel.Reset(); BoundHUDPageGeneration = 0;
    if (Previous.IsValid()) Previous->OnScreenDeactivated();

    Super::UnbindUIEvents();
}

void UDivineBeastsHUDWidget::RefreshInitialState()
{ ReconcileHUDModel(); Super::RefreshInitialState(); }
void UDivineBeastsHUDWidget::ReconcileHUDModel()
{
    if (!bHUDEventsBound) return;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(Cast<UDivineBeastsUIViewModel>(GetViewModel()));
    const int32 Generation = Selected.IsValid() ? Selected->GetPageGeneration() : 0;
    if (BoundHUDModel.Get() == Selected.Get() && BoundHUDPageGeneration == Generation) return;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundHUDModel.Get());
    const uint64 Binding = ++HUDBindingGeneration;
    BoundHUDModel = Selected.Get(); BoundHUDPageGeneration = Generation;
    if (Previous.IsValid()) Previous->OnScreenDeactivated();
    // 取消旧命令可同步关闭/替换HUD；后继绑定优先，不能在回调后复活本次旧选择。
    if (bHUDEventsBound && Binding == HUDBindingGeneration && GetViewModel() == Selected.Get() &&
        Selected.IsValid() && Selected->IsPageActive() && Selected->GetPageGeneration() == Generation)
        Selected->OnScreenActivated();
}
