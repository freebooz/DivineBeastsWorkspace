#include "ViewModels/Domains/DivineBeastsScopedDomainViewModelBase.h"

bool UDivineBeastsScopedDomainViewModelBase::BeginSourceScope(const FGuid& NewScopeId)
{
    if (!NewScopeId.IsValid())
    {
        return false;
    }
    if (SourceScopeId == NewScopeId)
    {
        return true;
    }
    SourceScopeId = NewScopeId;
    SourceRevision = -1;
    ResetDomainProjection();
    MarkStateChanged();
    return true;
}

void UDivineBeastsScopedDomainViewModelBase::ClearSourceScope()
{
    if (!SourceScopeId.IsValid())
    {
        return;
    }
    SourceScopeId.Invalidate();
    SourceRevision = -1;
    ResetDomainProjection();
    MarkStateChanged();
}

bool UDivineBeastsScopedDomainViewModelBase::CanAcceptSnapshot(
    const FGuid& ScopeId, int64 InSnapshotRevision) const
{
    // 拒绝旧账号、未知来源以及迟到或重复的数据，不按客户端时钟推断权威新旧。
    return ScopeId.IsValid() && ScopeId == SourceScopeId &&
        InSnapshotRevision >= 0 && InSnapshotRevision > SourceRevision;
}

void UDivineBeastsScopedDomainViewModelBase::PublishAcceptedSnapshot(
    int64 InSnapshotRevision)
{
    SourceRevision = InSnapshotRevision;
    MarkStateChanged();
}
