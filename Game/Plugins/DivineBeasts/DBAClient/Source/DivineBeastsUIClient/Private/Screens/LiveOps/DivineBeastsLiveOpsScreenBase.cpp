#include "Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h"
#include "ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

UDivineBeastsLiveOpsViewModel* UDivineBeastsLiveOpsScreenBase::GetLiveOpsViewModel() const
{
    return Cast<UDivineBeastsLiveOpsViewModel>(GetViewModel());
}
FDivineBeastsLiveOpsUIProjection UDivineBeastsLiveOpsScreenBase::GetLiveOpsProjection() const
{
    const UDivineBeastsLiveOpsViewModel* VM = GetLiveOpsViewModel();
    return VM ? VM->GetSnapshotRef() : FDivineBeastsLiveOpsUIProjection();
}

void UDivineBeastsLiveOpsScreenBase::BindUIEvents()
{
    Super::BindUIEvents();
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("NoticesButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::ShowNotices);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::CloseNotices);
    if (auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("NoticesList")))) List->OnSelectionChanged.AddUniqueDynamic(this,&ThisClass::NoticeSelected);
}
void UDivineBeastsLiveOpsScreenBase::UnbindUIEvents()
{
    for (const TCHAR* Name:{TEXT("NoticesButton"),TEXT("CloseButton")}) if (auto* Button=Cast<UButton>(GetWidgetFromName(Name))) Button->OnClicked.RemoveAll(this);
    if (auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("NoticesList")))) List->OnSelectionChanged.RemoveAll(this);
    NoticeIds.Reset(); SelectedNoticeId=NAME_None; Super::UnbindUIEvents();
}
void UDivineBeastsLiveOpsScreenBase::RefreshInitialState() { Super::RefreshInitialState(); RefreshNoticePresentation(); }
void UDivineBeastsLiveOpsScreenBase::OnViewModelStateChanged(int32 Revision,int32 Generation)
{ Super::OnViewModelStateChanged(Revision,Generation); RefreshNoticePresentation(); }
void UDivineBeastsLiveOpsScreenBase::ShowNotices() { RefreshNoticePresentation(); }
void UDivineBeastsLiveOpsScreenBase::RefreshNoticePresentation()
{
    if (!IsActivated()) return;
    const auto Projection=GetLiveOpsProjection(); TGuardValue<bool> Guard(bRefreshingNotices,true);
    NoticeIds.Reset(); auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("NoticesList")));
    if (List) List->ClearOptions();
    if (Projection.bServiceAvailable) for (const auto& Notice:Projection.Notices)
    { NoticeIds.Add(Notice.NoticeId); if (List) List->AddOption(FString::Printf(TEXT("%d · %s%s"),NoticeIds.Num(),Notice.bUnread?TEXT("未读 · "):TEXT(""),*Notice.Title.ToString())); }
    if (!NoticeIds.Contains(SelectedNoticeId)) SelectedNoticeId=NoticeIds.IsEmpty()?NAME_None:NoticeIds[0];
    const int32 Index=NoticeIds.IndexOfByKey(SelectedNoticeId);
    if (List) { List->SetIsEnabled(!NoticeIds.IsEmpty()); if (Index!=INDEX_NONE) List->SetSelectedIndex(Index); }
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("NoticeBodyText")))) Text->SetText(
        !Projection.bServiceAvailable?NSLOCTEXT("SSLMUI","LiveOpsUnavailable","公告服务尚未接入，未读取到真实公告。"):
        Index==INDEX_NONE?NSLOCTEXT("SSLMUI","NoNotices","当前没有公告。"):
        FText::FromString(Projection.Notices[Index].Title.ToString()+TEXT("\n\n")+Projection.Notices[Index].Summary.ToString()));
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("UnreadCountText")))) Text->SetText(FText::FromString(
        Projection.bServiceAvailable?FString::Printf(TEXT("未读 %d；浏览不自动回写已读状态"),Projection.UnreadCount):TEXT("未连接运营数据源")));
}
void UDivineBeastsLiveOpsScreenBase::NoticeSelected(FString,ESelectInfo::Type)
{
    if (bRefreshingNotices || !IsActivated()) return;
    const auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("NoticesList")));
    const int32 Index=List?List->GetSelectedIndex():INDEX_NONE;
    SelectedNoticeId=NoticeIds.IsValidIndex(Index)?NoticeIds[Index]:NAME_None; RefreshNoticePresentation();
}
void UDivineBeastsLiveOpsScreenBase::CloseNotices() { if (IsActivated()) CloseScreen(); }
