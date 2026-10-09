// 本文件属于GamePlatform平台层 GamePlatformUI，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "ViewModels/GamePlatformViewModelBase.h"

void UGamePlatformViewModelBase::BeginPage()
{
    ++PageGeneration;
    bPageActive = true;
    const int32 BeganGeneration = PageGeneration;

    // 先允许派生类订阅真实数据源并抓取初始快照，再统一广播首次状态。
    OnPageBegan();
    if (!bPageActive || PageGeneration != BeganGeneration) return; // 派生钩子自闭/开始新页后，不再广播旧代首次状态。
    MarkStateChanged();
}

void UGamePlatformViewModelBase::EndPage()
{
    // 先使旧异步回调失效；解绑钩子同步重入BeginPage时，新代次不被旧函数返回后覆盖。
    bPageActive = false;
    ++PageGeneration;
    OnPageEnded();
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
