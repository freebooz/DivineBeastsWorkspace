#include "Screens/Boot/DivineBeastsBootScreen.h"

#include "ViewModels/Boot/DivineBeastsBootViewModel.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UDivineBeastsBootViewModel*
UDivineBeastsBootScreen::GetBootViewModel() const
{
    // 页面只读取平台 Screen 已持有的唯一 ViewModel，避免再维护第二份状态所有权。
    return Cast<UDivineBeastsBootViewModel>(GetViewModel());
}

void UDivineBeastsBootScreen::RefreshInitialState()
{ Super::RefreshInitialState(); RefreshBootPresentation(); }
void UDivineBeastsBootScreen::OnViewModelStateChanged(int32 Revision, int32 Generation)
{ Super::OnViewModelStateChanged(Revision, Generation); RefreshBootPresentation(); }
void UDivineBeastsBootScreen::RefreshBootPresentation()
{
    if (!IsActivated()) return;
    const auto* Model = GetBootViewModel();
    const auto Snapshot = Model ? Model->GetLoadingSnapshot() : FGamePlatformUILoadingSnapshot();
    const bool bKnown = Model && FMath::IsFinite(Snapshot.Progress) && Snapshot.Progress >= 0.0f;
    if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("StageText"))))
        Text->SetText(Snapshot.Stage.IsEmpty() ? NSLOCTEXT("SSLMUI", "BootPreparing", "正在初始化，请稍候") : Snapshot.Stage);
    if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("ProgressText"))))
        Text->SetText(bKnown ? FText::AsPercent(FMath::Clamp(Snapshot.Progress, 0.0f, 1.0f)) : NSLOCTEXT("SSLMUI", "BootUnknown", "等待真实加载进度"));
    if (auto* Progress = Cast<UProgressBar>(GetWidgetFromName(TEXT("LoadingProgress"))))
    { Progress->SetIsMarquee(!bKnown); if (bKnown) Progress->SetPercent(FMath::Clamp(Snapshot.Progress, 0.0f, 1.0f)); }
}
