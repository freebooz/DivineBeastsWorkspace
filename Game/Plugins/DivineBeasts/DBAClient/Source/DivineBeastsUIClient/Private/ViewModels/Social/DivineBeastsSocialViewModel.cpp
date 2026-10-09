#include "ViewModels/Social/DivineBeastsSocialViewModel.h"

bool UDivineBeastsSocialViewModel::ApplySocialSnapshot(
    const FDivineBeastsSocialUIProjection& Next)
{
    // 只接受已绑定账号的较新快照。上限保护蓝图序列化与滚动列表的内存占用。
    constexpr int32 MaxPartyMembers = 10;
    constexpr int32 MaxFriends = 256;
    if (!CanAcceptSnapshot(Next.SourceScopeId, Next.Revision) ||
        Next.PartyMembers.Num() > MaxPartyMembers ||
        Next.Friends.Num() > MaxFriends)
    {
        return false;
    }
    Snapshot = Next;
    PublishAcceptedSnapshot(Next.Revision);
    return true;
}
