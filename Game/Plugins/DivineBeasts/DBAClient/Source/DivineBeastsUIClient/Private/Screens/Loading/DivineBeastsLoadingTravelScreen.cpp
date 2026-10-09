#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"

#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "Components/TextBlock.h"

UDivineBeastsLoadingViewModel*
UDivineBeastsLoadingTravelScreen::GetLoadingViewModel() const
{
    // 使用平台 Screen 已持有的唯一 ViewModel，不复制加载状态所有权。
    return Cast<UDivineBeastsLoadingViewModel>(GetViewModel());
}

void UDivineBeastsLoadingTravelScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (auto* LocalViewModel = GetLoadingViewModel())
        LocalViewModel->OnViewStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshLoadingPresentation);
    RefreshLoadingPresentation(0, 0);
}

void UDivineBeastsLoadingTravelScreen::NativeOnDeactivated()
{
    if (auto* LocalViewModel = GetLoadingViewModel())
        LocalViewModel->OnViewStateChanged.RemoveDynamic(this, &ThisClass::RefreshLoadingPresentation);
    Super::NativeOnDeactivated();
}

void UDivineBeastsLoadingTravelScreen::RefreshLoadingPresentation(int32, int32)
{
    auto* LocalViewModel = GetLoadingViewModel();
    if (!StageText || !LocalViewModel) return;
    const auto Snapshot = LocalViewModel->GetLoadingSnapshot();
    StageText->SetText(Snapshot.Stage.IsEmpty()
        ? NSLOCTEXT("DivineBeastsUI", "PreparingWorld", "正在准备世界，请稍候") : Snapshot.Stage);
}
