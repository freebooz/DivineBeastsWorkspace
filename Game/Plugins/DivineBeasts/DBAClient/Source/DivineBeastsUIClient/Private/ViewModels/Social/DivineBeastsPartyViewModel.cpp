// 项目公共小队桥：只消费平台DTO与认证变化；失败/取消/换账号以代次隔离，未接入的实时观察与限时准备明确不可用。
#include "ViewModels/Social/DivineBeastsPartyViewModel.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

void UDivineBeastsPartyViewModel::ConnectPlayer(ULocalPlayer* Player)
{
    DisconnectPlayer();
    if (!Player || !Player->GetGameInstance()) return;
    Online=Player->GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
    RequestWorld=Player->GetWorld();
    if (auto* Service=Online.Get())
    {
        AuthHandle=Service->OnAuthStateChanged().AddUObject(this,&UDivineBeastsPartyViewModel::AuthenticationChanged);
        AuthenticationChanged(Service->GetSnapshot());
    }
}
void UDivineBeastsPartyViewModel::CancelOwnedRequests()
{
    // 先撤身份再取消，取消通知允许重入；旧回调只能完成自己的等待，不能写新账号。
    ++OperationGeneration; PendingReads=0; bBusy=false;
    auto Requests=MoveTemp(OwnedRequests); OwnedRequests.Reset();
    if (auto* Service=Online.Get()) for (const auto& Request:Requests) Service->Cancel(Request);
}
void UDivineBeastsPartyViewModel::DisconnectPlayer()
{
    CancelOwnedRequests();
    if (auto* Service=Online.Get()) Service->OnAuthStateChanged().Remove(AuthHandle);
    AuthHandle.Reset(); Online.Reset(); RequestWorld.Reset(); AuthScope.Invalidate(); CurrentPlayerId.Empty();
    Party={}; Invitations.Reset(); bStateKnown=false; bOutcomeUnknown=false; UnknownCommand={};
    TargetPlayer.Empty(); TargetParty.Empty(); TargetRevision=-1; ClearSourceScope();
}
void UDivineBeastsPartyViewModel::BeginDestroy() { DisconnectPlayer(); Super::BeginDestroy(); }
void UDivineBeastsPartyViewModel::AuthenticationChanged(const FGamePlatformAuthSnapshot& AuthSnapshot)
{
    const auto* Service=Online.Get();
    const bool bAuthenticated=AuthSnapshot.State==EGamePlatformAuthState::Authenticated || AuthSnapshot.State==EGamePlatformAuthState::Refreshing;
    const FString PlayerId=Service ? Service->GetAuthentication().PlayerId : FString();
    if (bAuthenticated && AuthSnapshot.AuthGeneration==AuthScope && CurrentPlayerId==PlayerId) return;
    CancelOwnedRequests(); AuthScope=AuthSnapshot.AuthGeneration;
    Party={}; Invitations.Reset(); bStateKnown=false; bOutcomeUnknown=false; UnknownCommand={};
    TargetPlayer.Empty(); TargetParty.Empty(); TargetRevision=-1; ProjectionRevision=0;
    ClearSourceScope(); CurrentPlayerId=bAuthenticated ? PlayerId : FString();
    if (bAuthenticated && AuthScope.IsValid() && !CurrentPlayerId.IsEmpty()) BeginSourceScope(AuthScope);
    StatusText=FText::FromString(bAuthenticated ? TEXT("读取小队状态") : TEXT("请先登录"));
    MarkStateChanged();
    if (IsPageActive() && HasActiveSourceScope()) RefreshParty();
}
void UDivineBeastsPartyViewModel::OnPageBegan()
{
    Super::OnPageBegan();
    if(auto* Service=Online.Get())
    {
        if(!AuthHandle.IsValid()) AuthHandle=Service->OnAuthStateChanged().AddUObject(this,&UDivineBeastsPartyViewModel::AuthenticationChanged);
        AuthenticationChanged(Service->GetSnapshot());
    }
    RefreshParty();
}
void UDivineBeastsPartyViewModel::OnPageEnded()
{
    CancelOwnedRequests(); bStateKnown=false;
    if(auto* Service=Online.Get())Service->OnAuthStateChanged().Remove(AuthHandle);
    AuthHandle.Reset();Super::OnPageEnded();
}
void UDivineBeastsPartyViewModel::RefreshParty()
{
    auto* Service=Online.Get();
    if (!Service || !RequestWorld.IsValid() || !IsPageActive() || !HasActiveSourceScope() || bBusy) return;
    const uint64 Generation=++OperationGeneration; const FGuid Scope=AuthScope;
    const TWeakObjectPtr<UDivineBeastsPartyViewModel> Weak(this);
    bBusy=true; bStateKnown=false; PendingReads=2; bReadFailed=false; OwnedRequests.Reset();
    StatusText=FText::FromString(TEXT("正在读取；其他玩家的变更请使用刷新按钮确认")); MarkStateChanged();
    if (!IsPageActive() || OperationGeneration!=Generation || AuthScope!=Scope) return;
    FGamePlatformOnlineRequestOptions Options; Options.Owner=this; Options.Lifetime=EGamePlatformOnlineRequestLifetime::World; Options.World=RequestWorld;
    auto Complete=[Weak,Generation,Scope](const FGamePlatformPartyResult& Result,bool bInbox)
    {
        auto* Self=Weak.Get(); if (!Self || !Self->IsPageActive() || Self->OperationGeneration!=Generation || Self->AuthScope!=Scope) return;
        if (Result.bSuccess)
        {
            if (bInbox) Self->Invitations=Result.Invitations;
            else { Self->bStateKnown=Self->ApplyParty(Result.Snapshot); Self->bReadFailed|=!Self->bStateKnown; }
        }
        else { Self->bReadFailed=true; Self->ReportFailure(Result); }
        if (--Self->PendingReads==0)
        {
            Self->bBusy=false; Self->OwnedRequests.Reset();
            Self->bStateKnown&=!Self->bReadFailed;
            if (Self->bStateKnown && !Self->bOutcomeUnknown) Self->StatusText=FText::FromString(TEXT("小队状态已更新；实时观察接口尚未提供，可手动刷新"));
        }
        Self->PublishPartyProjection();
    };
    const auto Read=Service->GetCurrentParty(Options,[Complete](const FGamePlatformPartyResult& R){Complete(R,false);});
    if (!IsPageActive() || OperationGeneration!=Generation || AuthScope!=Scope) { Service->Cancel(Read); return; }
    OwnedRequests.Add(Read);
    const auto Inbox=Service->GetPartyInvitations(Options,[Complete](const FGamePlatformPartyResult& R){Complete(R,true);});
    if (!IsPageActive() || OperationGeneration!=Generation || AuthScope!=Scope) { Service->Cancel(Inbox); return; }
    OwnedRequests.Add(Inbox);
}
bool UDivineBeastsPartyViewModel::ApplyParty(const FGamePlatformPartySnapshot& Next)
{
    if (Next.Members.Num()>5 || Next.Requests.Num()>64 ||
        (Party.bHasParty && Next.PartyId==Party.PartyId && Next.Revision<Party.Revision)) return false;
    if (Next.bHasParty && !Next.Members.ContainsByPredicate([this](const auto& M){return M.PlayerId==CurrentPlayerId;})) return false;
    Party=Next;ServerSnapshotMonotonicSeconds=FPlatformTime::Seconds(); return true;
}
FDateTime UDivineBeastsPartyViewModel::GetEstimatedServerTimeForDisplay() const
{
    return Party.ServerTimeUtc+FTimespan::FromSeconds(FMath::Max(0.0,FPlatformTime::Seconds()-ServerSnapshotMonotonicSeconds));
}
void UDivineBeastsPartyViewModel::PublishPartyProjection()
{
    FDivineBeastsSocialUIProjection Next; Next.SourceScopeId=AuthScope; Next.Revision=++ProjectionRevision;
    Next.bServiceAvailable=bStateKnown; Next.PartyId=Party.PartyId.IsEmpty() ? NAME_None : FName(*Party.PartyId);
    for (const auto& Member:Party.Members)
    {
        FDivineBeastsSocialMemberView View; View.PublicPlayerId=Member.PlayerId;
        View.DisplayName=FText::FromString(Member.DisplayName.IsEmpty()?Member.PlayerId:Member.DisplayName);
        View.bOnline=Member.bOnline; View.bPartyMember=true; Next.PartyMembers.Add(MoveTemp(View));
    }
    if (!ApplySocialSnapshot(Next)) MarkStateChanged();
}
void UDivineBeastsPartyViewModel::ReportFailure(const FGamePlatformPartyResult& Result)
{
    bOutcomeUnknown|=Result.bOutcomeUnknown;
    if (Result.bOutcomeUnknown) StatusText=FText::FromString(TEXT("操作结果未知，请先刷新；重试将沿用原请求身份"));
    else if (Result.ErrorCode==TEXT("Party.Conflict")) StatusText=FText::FromString(TEXT("队伍状态已变化，请刷新后重新选择"));
    else if (Result.ErrorCode==TEXT("Party.Forbidden")) StatusText=FText::FromString(TEXT("没有执行该操作的权限"));
    else if (Result.ErrorCode==TEXT("Party.Cancelled")) StatusText=FText::FromString(TEXT("本地等待已取消；远端是否提交需刷新确认"));
    else StatusText=FText::FromString(TEXT("小队服务未就绪或响应无效，请稍后刷新"));
}
bool UDivineBeastsPartyViewModel::CanSubmitPartyAction(const FString& Action) const
{
    if (!Online.IsValid() || !IsPageActive() || !HasActiveSourceScope() || bBusy || !bStateKnown || bOutcomeUnknown) return false;
    if (Action==TEXT("acceptRequest") || Action==TEXT("declineRequest")) return !Invitations.IsEmpty() || (Party.bHasParty && !Party.bRosterLocked && Party.LeaderPlayerId==CurrentPlayerId);
    if (Action==TEXT("requestJoin")) return !Party.bHasParty && !TargetParty.IsEmpty() && TargetRevision>=0;
    if (!Party.bHasParty || Party.bRosterLocked) return false;
    if (Action==TEXT("leave") || Action==TEXT("ready")) return true;
    return Party.LeaderPlayerId==CurrentPlayerId && (Action==TEXT("invite") || Action==TEXT("kick") || Action==TEXT("transferLeader") || Action==TEXT("disband") || Action==TEXT("cancelRequest"));
}
void UDivineBeastsPartyViewModel::SetTargetContext(const FString& PlayerId,const FString& PartyId,int64 ContextRevision)
{ TargetPlayer=PlayerId; TargetParty=PartyId; TargetRevision=ContextRevision; MarkStateChanged(); }
bool UDivineBeastsPartyViewModel::SubmitPartyAction(const FString& Action,const FString& TargetPlayerId,const FString& RequestKey)
{
    if (!CanSubmitPartyAction(Action)) { StatusText=FText::FromString(TEXT("当前状态不允许该操作；需已知队伍、正确权限及未锁定名单")); MarkStateChanged(); return false; }
    FGamePlatformPartyCommand Command; Command.Action=Action; Command.PartyId=Party.PartyId; Command.ExpectedRevision=Party.Revision;
    Command.TargetPlayerId=TargetPlayerId.IsEmpty()?TargetPlayer:TargetPlayerId;
    Command.ClientRequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    if (Action==TEXT("requestJoin")) { Command.PartyId=TargetParty; Command.ExpectedRevision=TargetRevision; }
    if (Action==TEXT("ready"))
    {
        const auto* Self=Party.Members.FindByPredicate([this](const auto& M){return M.PlayerId==CurrentPlayerId;});
        if (!Self) return false; Command.bReady=!Self->bReady;
    }
    if (Action==TEXT("acceptRequest") || Action==TEXT("declineRequest") || Action==TEXT("cancelRequest"))
    {
        // 选择键必须属于本次授权快照，绝不把列表首项当成玩家已经选择。
        const auto* Selected=Invitations.FindByPredicate([&](const auto& R){return R.PartyId+TEXT("\n")+R.RequestId==RequestKey;});
        if (!Selected) Selected=Party.Requests.FindByPredicate([&](const auto& R){return R.RequestId==RequestKey && R.State==TEXT("pending");});
        if (!Selected) { StatusText=FText::FromString(TEXT("请选择仍有效的邀请或申请")); MarkStateChanged(); return false; }
        Command.PartyId=Selected->PartyId; Command.ExpectedRevision=Selected->Revision; Command.MembershipRequestId=Selected->RequestId;
    }
    return Dispatch(Command);
}
bool UDivineBeastsPartyViewModel::Dispatch(const FGamePlatformPartyCommand& Command)
{
    auto* Service=Online.Get(); if (!Service || !RequestWorld.IsValid() || bBusy || !IsPageActive()) return false;
    bBusy=true; const uint64 Generation=++OperationGeneration; const FGuid Scope=AuthScope;
    const TWeakObjectPtr<UDivineBeastsPartyViewModel> Weak(this); FGamePlatformOnlineRequestOptions Options; Options.Owner=this;
    Options.Lifetime=EGamePlatformOnlineRequestLifetime::World;Options.World=RequestWorld;
    StatusText=FText::FromString(TEXT("正在等待服务器确认")); MarkStateChanged();
    if (!IsPageActive() || OperationGeneration!=Generation || AuthScope!=Scope) return false;
    const auto Request=Service->CommandParty(Command,Options,[Weak,Generation,Scope,Command](const FGamePlatformPartyResult& Result)
    {
        auto* Self=Weak.Get(); if (!Self || !Self->IsPageActive() || Self->OperationGeneration!=Generation || Self->AuthScope!=Scope) return;
        Self->bBusy=false; Self->OwnedRequests.Reset();
        if (Result.bSuccess && Self->ApplyParty(Result.Snapshot))
        { Self->bStateKnown=true; Self->bOutcomeUnknown=false; Self->UnknownCommand={}; Self->StatusText=FText::FromString(TEXT("服务器已确认操作")); }
        else { if (Result.bOutcomeUnknown) Self->UnknownCommand=Command; Self->ReportFailure(Result); Self->bStateKnown=false; }
        Self->PublishPartyProjection();
        if (Result.bSuccess) Self->RefreshParty();
    });
    if(!IsPageActive() || OperationGeneration!=Generation || AuthScope!=Scope){Service->Cancel(Request);return false;}
    OwnedRequests.Add(Request);
    return true;
}
bool UDivineBeastsPartyViewModel::RetryUnknownCommand()
{
    if (!bOutcomeUnknown || !bStateKnown || bBusy || !HasActiveSourceScope()) return false;
    return Dispatch(UnknownCommand);
}
