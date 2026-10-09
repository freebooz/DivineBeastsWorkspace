#include "ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h"

bool UDivineBeastsLiveOpsViewModel::ApplyLiveOpsSnapshot(
    const FDivineBeastsLiveOpsUIProjection& Next)
{
    // 单次仅展示有限条公告；分页归运营数据客户端，避免UI无界缓存。
    constexpr int32 MaxNotices = 128;
    if (!CanAcceptSnapshot(Next.SourceScopeId, Next.Revision) ||
        Next.UnreadCount < 0 ||
        Next.Notices.Num() > MaxNotices)
    {
        return false;
    }
    Snapshot = Next;
    PublishAcceptedSnapshot(Next.Revision);
    return true;
}
