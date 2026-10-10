// 项目公共页面只消费平台安全钩子；OnScreen业务订阅精确属于绑定VM与页面代次，不另建世界/网络机制。
#include "Screens/DivineBeastsUIScreen.h"

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"

void UDivineBeastsUIScreen::ShowPageLoadError(const FText& Message)
{
    // 仅使用Monolith资产已有的命名控件，禁止在C++动态构造另一套视觉界面。
    PageLoadError = Message;
    if (UTextBlock* Error = Cast<UTextBlock>(GetWidgetFromName(TEXT("ErrorText"))))
    {
        Error->SetText(Message);
        Error->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

// 项目VM的Owner状态订阅只归本页激活作用域；所有回调返回都保留后继选择优先权。
void UDivineBeastsUIScreen::BindUIEvents()
{
    Super::BindUIEvents();
    if (!IsActivated()) return;
    bProjectEventsBound = true;
    ReconcileProjectViewModelBinding();
}

void UDivineBeastsUIScreen::UnbindUIEvents()
{
    const TStrongObjectPtr<UDivineBeastsUIScreen> KeepScreen(this);
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundProjectViewModel);
    // 先撤本次资格和精确VM引用，再进入可能取消命令/重新激活页面的外部业务解绑。
    bProjectEventsBound = false;
    ++ProjectBindingGeneration;
    BoundProjectViewModel = nullptr;
    BoundProjectPageGeneration = 0;
    if (Previous.IsValid()) Previous->OnScreenDeactivated();
    Super::UnbindUIEvents();
}

bool UDivineBeastsUIScreen::ReconcileProjectViewModelBinding()
{
    if (!IsActivated() || !bProjectEventsBound) return false;
    const TStrongObjectPtr<UDivineBeastsUIScreen> KeepScreen(this);
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(Cast<UDivineBeastsUIViewModel>(GetViewModel()));
    const int32 PageGeneration = Selected.IsValid() ? Selected->GetPageGeneration() : 0;
    if (Selected.IsValid() && !Selected->IsPageActive()) return false;
    if (BoundProjectViewModel == Selected.Get() && BoundProjectPageGeneration == PageGeneration) return true;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundProjectViewModel);
    const uint64 BindingGeneration = ++ProjectBindingGeneration;
    BoundProjectViewModel = Selected.Get();
    BoundProjectPageGeneration = PageGeneration;
    const auto IsCurrent = [this, BindingGeneration, PageGeneration, VM = Selected.Get()]()
    {
        return IsActivated() && bProjectEventsBound && ProjectBindingGeneration == BindingGeneration &&
            GetViewModel() == VM && BoundProjectViewModel == VM && BoundProjectPageGeneration == PageGeneration &&
            (!VM || (VM->IsPageActive() && VM->GetPageGeneration() == PageGeneration));
    };
    if (Previous.IsValid()) Previous->OnScreenDeactivated();
    if (!IsCurrent()) return false;
    if (Selected.IsValid()) Selected->OnScreenActivated();
    return IsCurrent();
}

void UDivineBeastsUIScreen::RefreshInitialState()
{
    if (ReconcileProjectViewModelBinding()) Super::RefreshInitialState();
}

void UDivineBeastsUIScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    if (ReconcileProjectViewModelBinding()) Super::OnViewModelStateChanged(Revision, PageGeneration);
}
