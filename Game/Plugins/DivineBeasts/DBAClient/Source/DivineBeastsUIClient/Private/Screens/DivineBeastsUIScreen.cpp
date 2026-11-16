// 项目公共页面只消费平台安全钩子；OnScreen业务订阅精确属于绑定VM与页面代次，不另建世界/网络机制。
#include "Screens/DivineBeastsUIScreen.h"

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "DivineBeastsPresentationClientSubsystem.h"

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
    // 页面可能在VM同步回调中失活，只有仍有效的页面才能附加声音观察者。
    if (IsActivated() && bProjectEventsBound) BindButtonAudio();
}

void UDivineBeastsUIScreen::UnbindUIEvents()
{
    const TStrongObjectPtr<UDivineBeastsUIScreen> KeepScreen(this);
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundProjectViewModel);
    // 先撤本次资格和精确VM引用，再进入可能取消命令/重新激活页面的外部业务解绑。
    bProjectEventsBound = false;
    UnbindButtonAudio();
    ++ProjectBindingGeneration;
    BoundProjectViewModel = nullptr;
    BoundProjectPageGeneration = 0;
    if (Previous.IsValid()) Previous->OnScreenDeactivated();
    Super::UnbindUIEvents();
}

/** 仅页面激活时枚举已有WidgetTree，不扫描Tick；按钮原命令委托完全不受影响。 */
void UDivineBeastsUIScreen::BindButtonAudio()
{
    if (!IsActivated() || !WidgetTree) return;
    WidgetTree->ForEachWidgetAndDescendants([this](UWidget* Widget)
    {
        UButton* Button = Cast<UButton>(Widget);
        if (!IsValid(Button)) return;
        const FString Name = Button->GetFName().ToString();
        const bool bCancel = Name.Contains(TEXT("Cancel"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Back"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Close"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Revert"), ESearchCase::IgnoreCase);
        const bool bConfirm = !bCancel &&
            (Name.Contains(TEXT("Confirm"), ESearchCase::IgnoreCase) ||
             Name.Contains(TEXT("Accept"), ESearchCase::IgnoreCase) ||
             Name.Contains(TEXT("Apply"), ESearchCase::IgnoreCase) ||
             Name.Contains(TEXT("Save"), ESearchCase::IgnoreCase));
        if (bCancel)
        {
            Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonAudioCancel);
            CancelAudioButtons.AddUnique(Button);
        }
        else if (bConfirm)
        {
            Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonAudioConfirm);
            ConfirmAudioButtons.AddUnique(Button);
        }
        else
        {
            Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonAudioClick);
            ClickAudioButtons.AddUnique(Button);
        }
    });
}

/** 按钮所属页面退出后解绑音频观察者，不取消原按钮的业务委托。 */
void UDivineBeastsUIScreen::UnbindButtonAudio()
{
    for (const auto& Weak : ClickAudioButtons)
        if (UButton* Button = Weak.Get())
            Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonAudioClick);
    for (const auto& Weak : ConfirmAudioButtons)
        if (UButton* Button = Weak.Get())
            Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonAudioConfirm);
    for (const auto& Weak : CancelAudioButtons)
        if (UButton* Button = Weak.Get())
            Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonAudioCancel);
    ClickAudioButtons.Reset();
    ConfirmAudioButtons.Reset();
    CancelAudioButtons.Reset();
}

void UDivineBeastsUIScreen::PlayButtonAudio(FName ActionId)
{
    // 鼠标/触摸/手柄对同一UButton均只使用一次OnClicked；程序化提交不产生假点击声。
    if (!IsActivated() || !bProjectEventsBound) return;
    if (ULocalPlayer* Player = GetOwningLocalPlayer())
        if (auto* Audio = Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>())
            Audio->PlayStarterUISound(ActionId);
}
void UDivineBeastsUIScreen::HandleButtonAudioClick() { PlayButtonAudio(TEXT("Click")); }
void UDivineBeastsUIScreen::HandleButtonAudioConfirm() { PlayButtonAudio(TEXT("Confirm")); }
void UDivineBeastsUIScreen::HandleButtonAudioCancel() { PlayButtonAudio(TEXT("Cancel")); }

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
