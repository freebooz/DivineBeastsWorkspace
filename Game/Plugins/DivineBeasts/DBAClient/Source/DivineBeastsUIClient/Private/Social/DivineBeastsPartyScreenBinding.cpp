// 只把VM事件投影到Monolith命名控件；按钮提交意图，不发送HTTP、不切地图、不修改名单。
#include "Social/DivineBeastsPartyScreenBinding.h"
#include "Social/DivineBeastsPartyUIClientSubsystem.h"
#include "ViewModels/Social/DivineBeastsPartyViewModel.h"
#include "Widgets/Social/DivineBeastsPartyListEntry.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

bool UDivineBeastsPartyScreenBinding::IsBound() const
{return Screen.IsValid()&&Model.IsValid()&&Screen->GetViewModel()==Model.Get();}
UCommonActivatableWidgetStack* UDivineBeastsPartyScreenBinding::GetOriginalStack() const {return OriginalStack.Get();}
UGamePlatformUIScreen* UDivineBeastsPartyScreenBinding::GetBoundScreen() const {return Screen.Get();}
void UDivineBeastsPartyScreenBinding::Bind(UGamePlatformUIScreen* InScreen,UDivineBeastsPartyViewModel* InModel,UDivineBeastsPartyUIClientSubsystem* InOwner,UCommonActivatableWidgetStack* InStack)
{
    Unbind(); Screen=InScreen; Model=InModel; Owner=InOwner;
    OriginalStack=InStack;
    TimerWorld=InScreen?InScreen->GetWorld():nullptr;
    if (!InScreen || !InModel) return;
    InModel->OnViewStateChanged.AddDynamic(this,&UDivineBeastsPartyScreenBinding::Refresh);
    InScreen->OnActivated().AddUObject(this,&UDivineBeastsPartyScreenBinding::Activated);
    InScreen->OnDeactivated().AddUObject(this,&UDivineBeastsPartyScreenBinding::Deactivated);
#define PARTY_BUTTON(Name,Method) if(auto* Button=Cast<UButton>(InScreen->GetWidgetFromName(TEXT(Name)))) Button->OnClicked.AddDynamic(this,&UDivineBeastsPartyScreenBinding::Method)
    PARTY_BUTTON("InviteButton",Invite); PARTY_BUTTON("InviteTargetButton",Invite);
    PARTY_BUTTON("AcceptButton",Accept); PARTY_BUTTON("ApproveButton",Accept);
    PARTY_BUTTON("DeclineButton",Decline); PARTY_BUTTON("RejectButton",Decline);
    PARTY_BUTTON("LeavePartyButton",Leave); PARTY_BUTTON("TransferLeaderButton",Transfer); PARTY_BUTTON("KickMemberButton",Kick);
    PARTY_BUTTON("DisbandButton",Disband); PARTY_BUTTON("ToggleReadyButton",ToggleReady);
    PARTY_BUTTON("RefreshButton",Reload); PARTY_BUTTON("CloseButton",Close);
    PARTY_BUTTON("ApplicationsButton",OpenApplications); PARTY_BUTTON("InvitationsButton",OpenInvitations);
    PARTY_BUTTON("ReadyCheckButton",OpenReady); PARTY_BUTTON("RequestJoinButton",RequestJoin);
    PARTY_BUTTON("ArenaButton",Activity); PARTY_BUTTON("RetryCommandButton",Retry);
#undef PARTY_BUTTON
    for(const TCHAR* Name:{TEXT("PartyMemberList"),TEXT("InvitationsList"),TEXT("ApplicationsList"),TEXT("ReadyRosterList")})
        if(auto* List=Cast<UListView>(InScreen->GetWidgetFromName(Name))) List->OnItemSelectionChanged().AddUObject(this,&UDivineBeastsPartyScreenBinding::SelectItem);
    Refresh();
}
void UDivineBeastsPartyScreenBinding::Unbind()
{
    const bool bOwnsCurrentModel=IsBound();
    if(auto* World=TimerWorld.Get())World->GetTimerManager().ClearTimer(DeadlineTimer);
    TimerWorld.Reset();SelectionOwnerPlayerId.Empty();
    if(auto* VM=Model.Get()) {VM->OnViewStateChanged.RemoveDynamic(this,&UDivineBeastsPartyScreenBinding::Refresh);VM->DisconnectPlayer();}
    if(auto* Page=Screen.Get())
    {
        Page->OnDeactivated().RemoveAll(this);
        Page->OnActivated().RemoveAll(this);
        for(const TCHAR* Name:{TEXT("InviteButton"),TEXT("InviteTargetButton"),TEXT("AcceptButton"),TEXT("ApproveButton"),TEXT("DeclineButton"),TEXT("RejectButton"),TEXT("LeavePartyButton"),TEXT("TransferLeaderButton"),TEXT("KickMemberButton"),TEXT("DisbandButton"),TEXT("ToggleReadyButton"),TEXT("RefreshButton"),TEXT("CloseButton"),TEXT("ApplicationsButton"),TEXT("InvitationsButton"),TEXT("ReadyCheckButton"),TEXT("RequestJoinButton"),TEXT("ArenaButton"),TEXT("RetryCommandButton")})
            if(auto* Button=Cast<UButton>(Page->GetWidgetFromName(Name))) Button->OnClicked.RemoveAll(this);
        for(const TCHAR* Name:{TEXT("PartyMemberList"),TEXT("InvitationsList"),TEXT("ApplicationsList"),TEXT("ReadyRosterList")})
            if(auto* List=Cast<UListView>(Page->GetWidgetFromName(Name))) { List->OnItemSelectionChanged().RemoveAll(this); if(bOwnsCurrentModel)List->ClearListItems(); }
    }
    Model.Reset(); Screen.Reset(); Owner.Reset();OriginalStack.Reset(); SelectedRequest.Empty(); SelectedMember.Empty();
}
void UDivineBeastsPartyScreenBinding::CloseOwnedPage()
{
    if(!IsBound()){Unbind();return;}
    const TStrongObjectPtr<UGamePlatformUIScreen> Page(Screen.Get());
    Unbind();if(Page.IsValid())Page->CloseScreen();
}
void UDivineBeastsPartyScreenBinding::Activated()
{
    if(!IsBound()){Unbind();return;}
    if(auto* VM=Model.Get())VM->OnViewStateChanged.AddUniqueDynamic(this,&UDivineBeastsPartyScreenBinding::Refresh);
    Refresh();
}
void UDivineBeastsPartyScreenBinding::Deactivated()
{
    // 层栈覆盖后仍可能重新激活同一实例：只暂停观察并清本地选择，不销毁下层页面的可重入绑定。
    if(auto* VM=Model.Get())VM->OnViewStateChanged.RemoveDynamic(this,&UDivineBeastsPartyScreenBinding::Refresh);
    SelectedRequest.Empty();SelectedMember.Empty();
    if(auto* World=TimerWorld.Get())World->GetTimerManager().ClearTimer(DeadlineTimer);
}
void UDivineBeastsPartyScreenBinding::Refresh(int32,int32)
{
    if(!IsBound()||bRebuildingLists)return;
    TGuardValue<bool> Rebuilding(bRebuildingLists,true);
    auto* Page=Screen.Get(); auto* VM=Model.Get(); if(!Page || !VM) return;
    if(SelectionOwnerPlayerId!=VM->CurrentPlayerId){SelectedRequest.Empty();SelectedMember.Empty();SelectionOwnerPlayerId=VM->CurrentPlayerId;}
    if(!SelectedMember.IsEmpty()&&!VM->Party.Members.ContainsByPredicate([this](const auto& Member){return Member.PlayerId==SelectedMember;}))SelectedMember.Empty();
    auto Text=[Page](const TCHAR* Name,const FText& Value){if(auto* Widget=Cast<UTextBlock>(Page->GetWidgetFromName(Name)))Widget->SetText(Value);};
    Text(TEXT("CommandErrorText"),VM->StatusText); Text(TEXT("PermissionText"),FText::FromString(VM->Party.bRosterLocked?TEXT("名单已锁定，必须等待后端取消匹配确认"):TEXT("操作权限由服务器复核")));
    Text(TEXT("TargetNameText"),FText::FromString(VM->GetTargetPlayerIdForDisplay()));
    Text(TEXT("MemberCountText"),FText::FromString(FString::Printf(TEXT("成员 %d / 5"),VM->Party.Members.Num())));
    Text(TEXT("ActivityText"),FText::FromString(TEXT("普通小队 · 实时变更请刷新确认")));
    Text(TEXT("ReadyStatusText"),FText::FromString(TEXT("限时小队准备会话尚未由后端提供；不以普通ready标记冒充")));
    Text(TEXT("RemainingSecondsText"),FText::FromString(TEXT("以服务器截止时间为准；刷新后校正")));
    auto Enabled=[Page](const TCHAR* Name,bool Value){if(auto* Widget=Page->GetWidgetFromName(Name))Widget->SetIsEnabled(Value);};
    Enabled(TEXT("InviteButton"),VM->CanSubmitPartyAction(TEXT("invite"))); Enabled(TEXT("InviteTargetButton"),VM->CanSubmitPartyAction(TEXT("invite")));
    for(const TCHAR* Name:{TEXT("AcceptButton"),TEXT("ApproveButton")})Enabled(Name,!SelectedRequest.IsEmpty()&&VM->CanSubmitPartyAction(TEXT("acceptRequest")));
    for(const TCHAR* Name:{TEXT("DeclineButton"),TEXT("RejectButton")})Enabled(Name,!SelectedRequest.IsEmpty()&&VM->CanSubmitPartyAction(TEXT("declineRequest")));
    Enabled(TEXT("LeavePartyButton"),VM->CanSubmitPartyAction(TEXT("leave")));
    Enabled(TEXT("TransferLeaderButton"),!SelectedMember.IsEmpty()&&VM->CanSubmitPartyAction(TEXT("transferLeader")));
    Enabled(TEXT("KickMemberButton"),!SelectedMember.IsEmpty()&&VM->CanSubmitPartyAction(TEXT("kick")));
    Enabled(TEXT("DisbandButton"),VM->CanSubmitPartyAction(TEXT("disband")));
    Enabled(TEXT("ToggleReadyButton"),VM->CanSubmitPartyAction(TEXT("ready")));
    Enabled(TEXT("RequestJoinButton"),VM->CanSubmitPartyAction(TEXT("requestJoin")));
    Enabled(TEXT("RefreshButton"),!VM->bBusy&&VM->HasActiveSourceScope());
    Enabled(TEXT("RetryCommandButton"),VM->bOutcomeUnknown&&VM->bStateKnown&&!VM->bBusy);
    Enabled(TEXT("ConfirmReadyButton"),false); Enabled(TEXT("DeclineReadyButton"),false);
    Enabled(TEXT("InspectCharacterButton"),false);
    Text(TEXT("DisabledReasonText"),FText::FromString(TEXT("申请需要目标队伍公开版本；查看他人角色合同尚未提供")));
    Enabled(TEXT("ArenaButton"),Owner.IsValid()&&Owner->HasActivity()&&VM->bStateKnown&&!VM->bBusy&&!VM->bOutcomeUnknown&&
        (!VM->Party.bHasParty||(!VM->Party.bRosterLocked&&VM->Party.LeaderPlayerId==VM->CurrentPlayerId)));
    auto Fill=[&](const TCHAR* Name,const TArray<TObjectPtr<UDivineBeastsPartyListItem>>& Items)
    {
        if(auto* List=Cast<UListView>(Page->GetWidgetFromName(Name)))
        {
            List->ClearListItems();
            for(const auto& Item:Items)
            {
                List->AddItem(Item.Get());
                const FString Key=Item->Key;
                if(Key==TEXT("member:")+SelectedMember||Key==TEXT("invitation:")+SelectedRequest||Key==TEXT("request:")+SelectedRequest)
                    List->SetSelectedItem(Item.Get());
            }
        }
    };
    TArray<TObjectPtr<UDivineBeastsPartyListItem>> Members,Inbox,Applications;
    for(const auto& Member:VM->Party.Members)
    {
        auto* Row=NewObject<UDivineBeastsPartyListItem>(this); Row->Key=TEXT("member:")+Member.PlayerId;
        Row->Title=FText::FromString((Member.PlayerId==VM->Party.LeaderPlayerId?TEXT("队长 · "):TEXT(""))+(Member.DisplayName.IsEmpty()?Member.PlayerId:Member.DisplayName));
        Row->Detail=FText::FromString(FString(Member.bOnline?TEXT("在线"):TEXT("离线"))+TEXT(" · ")+(Member.bReady?TEXT("已准备"):TEXT("未准备"))); Members.Add(Row);
    }
    for(const auto& Request:VM->Invitations)
    {
        auto* Row=NewObject<UDivineBeastsPartyListItem>(this); Row->Key=TEXT("invitation:")+Request.PartyId+TEXT("\n")+Request.RequestId;
        Row->Title=FText::FromString(Request.InviterPlayerId); Row->Detail=FText::FromString(FString::Printf(TEXT("邀请加入 · %d / 5 · 到期 %s UTC"),Request.MemberCount,*Request.ExpiresAtUtc.ToString(TEXT("%H:%M:%S")))); Inbox.Add(Row);
    }
    for(const auto& Request:VM->Party.Requests) if(Request.Kind==TEXT("application")&&Request.State==TEXT("pending"))
    {
        auto* Row=NewObject<UDivineBeastsPartyListItem>(this); Row->Key=TEXT("request:")+Request.RequestId;
        Row->Title=FText::FromString(Request.PlayerId); Row->Detail=FText::FromString(TEXT("申请加入 · 选择后批准或拒绝")); Applications.Add(Row);
    }
    Fill(TEXT("PartyMemberList"),Members); Fill(TEXT("ReadyRosterList"),Members); Fill(TEXT("InvitationsList"),Inbox); Fill(TEXT("ApplicationsList"),Applications);
    Text(TEXT("EmptyStateText"),FText::FromString(Applications.IsEmpty()?TEXT("暂无待处理申请"):TEXT("请选择一项申请，再批准或拒绝")));
    UpdateDeadlineDisplay();
}
void UDivineBeastsPartyScreenBinding::SelectItem(UObject* Object)
{
    if(bRebuildingLists||!IsBound())return;
    const auto* Item=Cast<UDivineBeastsPartyListItem>(Object);
    if(!Item){SelectedMember.Empty();SelectedRequest.Empty();Refresh();return;}
    if(Item->Key.StartsWith(TEXT("member:")))SelectedMember=Item->Key.Mid(7);
    else if(Item->Key.StartsWith(TEXT("invitation:")))SelectedRequest=Item->Key.Mid(11);
    else if(Item->Key.StartsWith(TEXT("request:")))SelectedRequest=Item->Key.Mid(8);
    Refresh();
    if(!SelectedRequest.IsEmpty())if(auto* World=TimerWorld.Get())World->GetTimerManager().SetTimer(DeadlineTimer,this,&UDivineBeastsPartyScreenBinding::UpdateDeadlineDisplay,0.25f,true);
}
void UDivineBeastsPartyScreenBinding::UpdateDeadlineDisplay()
{
    auto* Page=Screen.Get();auto* VM=Model.Get();if(!Page||!VM||!VM->IsPageActive())return;
    const auto* Request=VM->Invitations.FindByPredicate([&](const auto& R){return R.PartyId+TEXT("\n")+R.RequestId==SelectedRequest;});
    if(!Request)Request=VM->Party.Requests.FindByPredicate([&](const auto& R){return R.RequestId==SelectedRequest;});
    const double Remaining=Request&&VM->bStateKnown?FMath::Max(0.0,(Request->ExpiresAtUtc-VM->GetEstimatedServerTimeForDisplay()).GetTotalSeconds()):0.0;
    if(auto* T=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("RemainingSecondsText"))))T->SetText(FText::FromString(Request?FString::Printf(TEXT("剩余约 %.0f 秒 · 最终以服务器校验为准"),FMath::CeilToDouble(Remaining)):TEXT("请选择一项邀请")));
    if(Remaining<=0)
    {
        if(auto* World=TimerWorld.Get())World->GetTimerManager().ClearTimer(DeadlineTimer);
        for(const TCHAR* Name:{TEXT("AcceptButton"),TEXT("ApproveButton")})if(auto* B=Cast<UButton>(Page->GetWidgetFromName(Name)))B->SetIsEnabled(false);
    }
}
void UDivineBeastsPartyScreenBinding::Send(const FString& Action)
{
    if(!IsBound()){Unbind();return;}
    if(auto* VM=Model.Get())
    {
        // 再核真实ListView选中对象，不能让高亮B而缓存A的异常状态执行踢人/转让或批准。
        const bool bMemberAction=Action==TEXT("kick")||Action==TEXT("transferLeader");
        const bool bRequestAction=Action==TEXT("acceptRequest")||Action==TEXT("declineRequest");
        if(bMemberAction||bRequestAction)
        {
            auto* Page=Screen.Get();
            auto* List=Cast<UListView>(Page->GetWidgetFromName(bMemberAction?TEXT("PartyMemberList"):TEXT("InvitationsList")));
            if(!List&&bRequestAction)List=Cast<UListView>(Page->GetWidgetFromName(TEXT("ApplicationsList")));
            const auto* Item=List?Cast<UDivineBeastsPartyListItem>(List->GetSelectedItem()):nullptr;
            const FString Expected=bMemberAction?TEXT("member:")+SelectedMember:FString();
            const bool bMatches=Item&&(bMemberAction?Item->Key==Expected:
                Item->Key==TEXT("invitation:")+SelectedRequest||Item->Key==TEXT("request:")+SelectedRequest);
            if(!bMatches){VM->StatusText=FText::FromString(TEXT("请选择当前列表中的有效目标"));VM->MarkStateChanged();return;}
        }
        FString Target=SelectedMember;
        if(Action==TEXT("invite")) if(auto* Page=Screen.Get()) if(auto* Input=Cast<UEditableTextBox>(Page->GetWidgetFromName(TEXT("TargetPlayerInput"))))Target=Input->GetText().ToString();
        VM->SubmitPartyAction(Action,Target,SelectedRequest);
    }
}
void UDivineBeastsPartyScreenBinding::Invite(){Send(TEXT("invite"));}
void UDivineBeastsPartyScreenBinding::Accept(){Send(TEXT("acceptRequest"));}
void UDivineBeastsPartyScreenBinding::Decline(){Send(TEXT("declineRequest"));}
void UDivineBeastsPartyScreenBinding::Leave(){Send(TEXT("leave"));}
void UDivineBeastsPartyScreenBinding::Transfer(){Send(TEXT("transferLeader"));}
void UDivineBeastsPartyScreenBinding::Kick(){Send(TEXT("kick"));}
void UDivineBeastsPartyScreenBinding::Disband()
{
    if(auto* Page=Screen.Get()) if(auto* Confirm=Cast<UCheckBox>(Page->GetWidgetFromName(TEXT("ConfirmDisbandCheckBox")))) if(Confirm->IsChecked()) {Send(TEXT("disband"));return;}
    if(auto* VM=Model.Get()){VM->StatusText=FText::FromString(TEXT("解散不可自动撤回，请先勾选确认解散"));VM->MarkStateChanged();}
}
void UDivineBeastsPartyScreenBinding::ToggleReady(){Send(TEXT("ready"));}
void UDivineBeastsPartyScreenBinding::Reload(){if(auto* VM=Model.Get())VM->RefreshParty();}
void UDivineBeastsPartyScreenBinding::Retry(){if(auto* VM=Model.Get())VM->RetryUnknownCommand();}
void UDivineBeastsPartyScreenBinding::Close(){if(auto* Page=Screen.Get())Page->CloseScreen();}
void UDivineBeastsPartyScreenBinding::OpenApplications(){if(auto* Service=Owner.Get())Service->OpenPartyPage(TEXT("UI.Party.Applications"));}
void UDivineBeastsPartyScreenBinding::OpenInvitations(){if(auto* Service=Owner.Get())Service->OpenPartyPage(TEXT("UI.Party.Invitation"));}
void UDivineBeastsPartyScreenBinding::OpenReady(){if(auto* Service=Owner.Get())Service->OpenPartyPage(TEXT("UI.Party.Ready"));}
void UDivineBeastsPartyScreenBinding::RequestJoin(){Send(TEXT("requestJoin"));}
void UDivineBeastsPartyScreenBinding::Activity()
{
    auto* VM=Model.Get();auto* Service=Owner.Get();
    if(!VM||!Service||!VM->bStateKnown||VM->bBusy||VM->bOutcomeUnknown)return;
    if(VM->Party.bHasParty&&(VM->Party.bRosterLocked||VM->Party.LeaderPlayerId!=VM->CurrentPlayerId))return;
    if(!Service->OpenActivity(VM->Party.PartyId))
    {VM->StatusText=FText::FromString(TEXT("竞技活动入口当前不可用"));VM->MarkStateChanged();}
}
