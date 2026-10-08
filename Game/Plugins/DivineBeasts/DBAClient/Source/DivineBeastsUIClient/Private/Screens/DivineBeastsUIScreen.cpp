#include "Screens/DivineBeastsUIScreen.h"

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Components/TextBlock.h"

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

void UDivineBeastsUIScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenActivated();
    }
}

void UDivineBeastsUIScreen::NativeOnDeactivated()
{
    if (UDivineBeastsUIViewModel* ProjectViewModel =
        Cast<UDivineBeastsUIViewModel>(GetViewModel()))
    {
        ProjectViewModel->OnScreenDeactivated();
    }
    Super::NativeOnDeactivated();
}
