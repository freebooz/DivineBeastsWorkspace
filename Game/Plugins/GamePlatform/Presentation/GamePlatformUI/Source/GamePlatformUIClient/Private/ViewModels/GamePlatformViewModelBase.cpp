#include "ViewModels/GamePlatformViewModelBase.h"

void UGamePlatformViewModelBase::BeginPage()
{
    ++PageGeneration;
    bPageActive = true;

    // 先允许派生类订阅真实数据源并抓取初始快照，再统一广播首次状态。
    OnPageBegan();
    MarkStateChanged();
}

void UGamePlatformViewModelBase::EndPage()
{
    // 在 bPageActive 仍为 true 时解绑数据源，便于派生类按当前页面状态执行清理。
    OnPageEnded();
    bPageActive = false;
    ++PageGeneration;
}

void UGamePlatformViewModelBase::MarkStateChanged()
{
    ++Revision;
    OnViewStateChanged.Broadcast(Revision, PageGeneration);
}

bool UGamePlatformViewModelBase::IsCallbackCurrent(
    int32 ExpectedRevision,
    int32 ExpectedPageGeneration) const
{
    return bPageActive &&
           Revision == ExpectedRevision &&
           PageGeneration == ExpectedPageGeneration;
}
