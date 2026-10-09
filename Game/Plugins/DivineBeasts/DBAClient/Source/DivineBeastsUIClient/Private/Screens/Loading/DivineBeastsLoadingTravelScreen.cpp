// 项目层事件驱动加载页：只展示当前Loading VM快照；委托/激活/换VM所有权统一归平台基类。
// 不持有加载事务权威、不伪造进度；StageText仍由原BindWidget资产字段提供，本文件不生成或改资产。
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"

#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "Components/TextBlock.h"

UDivineBeastsLoadingViewModel*
UDivineBeastsLoadingTravelScreen::GetLoadingViewModel() const
{
    // 使用平台 Screen 已持有的唯一 ViewModel，不复制加载状态所有权。
    return Cast<UDivineBeastsLoadingViewModel>(GetViewModel());
}

void UDivineBeastsLoadingTravelScreen::RefreshInitialState()
{
    // 平台先验证激活/VM代次，替换VM也会经同一钩子刷新；不在Super激活通知返回后另建订阅。
    Super::RefreshInitialState();
    if (const auto* LocalViewModel = GetLoadingViewModel())
        RefreshLoadingPresentation(LocalViewModel->GetRevision(), LocalViewModel->GetPageGeneration());
}

void UDivineBeastsLoadingTravelScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    // 动态委托仅由平台基类持有；合法换VM精确拆旧/接新，派生只消费过滤后的快照。
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    RefreshLoadingPresentation(Revision, PageGeneration);
}

void UDivineBeastsLoadingTravelScreen::RefreshLoadingPresentation(int32, int32 PageGeneration)
{
    auto* LocalViewModel = GetLoadingViewModel();
    // 二次复核可见页面与捕获代次；失活/过期通知不能把文本恢复到旧页面。
    if (!IsActivated() || !StageText || !LocalViewModel || !LocalViewModel->IsPageActive() ||
        LocalViewModel->GetPageGeneration() != PageGeneration) return;
    const auto Snapshot = LocalViewModel->GetLoadingSnapshot();
    StageText->SetText(Snapshot.Stage.IsEmpty()
        ? NSLOCTEXT("DivineBeastsUI", "PreparingWorld", "正在准备世界，请稍候") : Snapshot.Stage);
}
