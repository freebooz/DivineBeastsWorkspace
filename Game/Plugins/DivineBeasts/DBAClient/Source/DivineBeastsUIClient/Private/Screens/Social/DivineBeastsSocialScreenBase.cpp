#include "Screens/Social/DivineBeastsSocialScreenBase.h"
#include "ViewModels/Social/DivineBeastsSocialViewModel.h"
#include "Social/DivineBeastsPartyUIClientSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"

UDivineBeastsSocialViewModel* UDivineBeastsSocialScreenBase::GetSocialViewModel() const
{
    return Cast<UDivineBeastsSocialViewModel>(GetViewModel());
}
FDivineBeastsSocialUIProjection UDivineBeastsSocialScreenBase::GetSocialProjection() const
{
    const UDivineBeastsSocialViewModel* VM = GetSocialViewModel();
    return VM ? VM->GetSnapshotRef() : FDivineBeastsSocialUIProjection();
}

void UDivineBeastsSocialScreenBase::BindUIEvents()
{
    Super::BindUIEvents();
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("FriendsButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::ShowFriends);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("PartyButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenParty);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::CloseSocialPage);
    if (auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("FriendsList")))) List->OnSelectionChanged.AddUniqueDynamic(this,&ThisClass::FriendSelected);
}
void UDivineBeastsSocialScreenBase::UnbindUIEvents()
{
    for (const TCHAR* Name:{TEXT("FriendsButton"),TEXT("PartyButton"),TEXT("CloseButton")})
        if (auto* Button=Cast<UButton>(GetWidgetFromName(Name))) Button->OnClicked.RemoveAll(this);
    if (auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("FriendsList")))) List->OnSelectionChanged.RemoveAll(this);
    PlayerIds.Reset(); SelectedPlayerId.Reset(); Super::UnbindUIEvents();
}
void UDivineBeastsSocialScreenBase::RefreshInitialState() { Super::RefreshInitialState(); RefreshSocialPresentation(); }
void UDivineBeastsSocialScreenBase::OnViewModelStateChanged(int32 Revision,int32 Generation)
{ Super::OnViewModelStateChanged(Revision,Generation); RefreshSocialPresentation(); }
void UDivineBeastsSocialScreenBase::ShowFriends() { RefreshSocialPresentation(); }
void UDivineBeastsSocialScreenBase::RefreshSocialPresentation()
{
    if (!IsActivated()) return;
    const auto Projection=GetSocialProjection();
    TGuardValue<bool> Guard(bRefreshingFriends,true); PlayerIds.Reset();
    auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("FriendsList")));
    if (List) List->ClearOptions();
    if (Projection.bServiceAvailable) for (const auto& Friend:Projection.Friends)
    {
        PlayerIds.Add(Friend.PublicPlayerId);
        // ComboBoxString使用显示文本作内部键；加入序号消除重名，业务身份另存独立数组。
        if (List) List->AddOption(FString::Printf(TEXT("%d · %s"),PlayerIds.Num(),*Friend.DisplayName.ToString()));
    }
    if (!PlayerIds.Contains(SelectedPlayerId)) SelectedPlayerId=PlayerIds.IsEmpty()?FString():PlayerIds[0];
    const int32 Index=PlayerIds.IndexOfByKey(SelectedPlayerId);
    if (List) { List->SetIsEnabled(!PlayerIds.IsEmpty()); if (Index!=INDEX_NONE) List->SetSelectedIndex(Index); }
    FString Detail=!Projection.bServiceAvailable?TEXT("好友服务尚未接入；不推测在线状态。小队功能使用独立的授权服务。"):
        Index==INDEX_NONE?TEXT("当前没有可展示的好友。"):
        Projection.Friends[Index].DisplayName.ToString()+(Projection.Friends[Index].bOnline?TEXT("\n在线"):TEXT("\n离线"));
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("SocialDetailText")))) Text->SetText(FText::FromString(Detail));
}
void UDivineBeastsSocialScreenBase::FriendSelected(FString,ESelectInfo::Type)
{
    if (bRefreshingFriends || !IsActivated()) return;
    const auto* List=Cast<UComboBoxString>(GetWidgetFromName(TEXT("FriendsList")));
    const int32 Index=List?List->GetSelectedIndex():INDEX_NONE;
    SelectedPlayerId=PlayerIds.IsValidIndex(Index)?PlayerIds[Index]:FString(); RefreshSocialPresentation();
}
void UDivineBeastsSocialScreenBase::OpenParty()
{
    if (!IsActivated()) return;
    auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsPartyUIClientSubsystem>():nullptr;
    if (Service && Service->OpenPartyPage()) { CloseScreen(); return; }
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("SocialDetailText"))))
        Text->SetText(Service&&!Service->LastError.IsEmpty()?Service->LastError:NSLOCTEXT("SSLMUI","PartyUnavailable","小队页面未受理，请检查当前世界和服务状态。"));
}
void UDivineBeastsSocialScreenBase::CloseSocialPage() { if (IsActivated()) CloseScreen(); }
