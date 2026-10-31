// 项目竞技本地玩家命令桥：把页面意图交给MOBA公开适配端口，绝不HTTP、直接切图或计算队列。
// 提供者来自GameInstance唯一真实匹配观察，能力由后端配置快照确认；页面失活不会取消队列。
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "Client/GamePlatformArenaMatchmakingAdapter.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
#include "Catalog/DivineBeastsArenaModeCatalog.h"

void UDivineBeastsArenaUIClientSubsystem::ConnectMatchmakingAuthentication()
{
    DisconnectMatchmakingAuthentication();
    auto* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
    auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    if (!Online) return;
    MatchmakingAuthenticationSource = Online;
    MatchmakingAuthenticationHandle = Online->OnAuthStateChanged().AddWeakLambda(this,
        [this](const FGamePlatformAuthSnapshot& Authentication)
    {
        if (MatchmakingRegistration.IsValid() && (Authentication.AuthGeneration != MatchmakingAuthGeneration ||
            (Authentication.State != EGamePlatformAuthState::Authenticated && Authentication.State != EGamePlatformAuthState::Refreshing)))
            ResetMatchmakingBridge();
        RefreshMatchmakingSource();
    });
    if(auto* Client=Instance->GetSubsystem<UDivineBeastsArenaClientSubsystem>())
    {MatchmakingSource=Client;MatchmakingSourceHandle=Client->OnMatchmakingStateChanged.AddUObject(this,&ThisClass::RefreshMatchmakingSource);}
    RefreshMatchmakingSource();
}

void UDivineBeastsArenaUIClientSubsystem::DisconnectMatchmakingAuthentication()
{
    if(auto* Source=MatchmakingSource.Get())Source->OnMatchmakingStateChanged.Remove(MatchmakingSourceHandle);
    MatchmakingSourceHandle.Reset();MatchmakingSource.Reset();
    if (auto* Online = MatchmakingAuthenticationSource.Get())
        Online->OnAuthStateChanged().Remove(MatchmakingAuthenticationHandle);
    MatchmakingAuthenticationSource.Reset();
    MatchmakingAuthenticationHandle.Reset();
}

FString UDivineBeastsArenaUIClientSubsystem::GetAuthenticatedMatchmakingPlayerId() const
{
    const auto* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
    const auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    if (!Online) return {};
    const auto State = Online->GetSnapshot().State;
    if (State != EGamePlatformAuthState::Authenticated && State != EGamePlatformAuthState::Refreshing) return {};
    return Online->GetAuthentication().PlayerId;
}

bool UDivineBeastsArenaUIClientSubsystem::IsMatchmakingAuthenticationCurrent() const
{
    const auto* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
    const auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    return Online && MatchmakingAuthGeneration.IsValid() &&
        Online->GetSnapshot().AuthGeneration == MatchmakingAuthGeneration &&
        !MatchmakingPlayerId.IsEmpty() && GetAuthenticatedMatchmakingPlayerId() == MatchmakingPlayerId;
}

FGuid UDivineBeastsArenaUIClientSubsystem::RegisterMatchmakingAdapter(TSharedPtr<IGamePlatformArenaMatchmakingAdapter> Adapter)
{
    if (!IsInGameThread() || bDeinitializing || MatchmakingAdapter || !Adapter) return {};
    MatchmakingAdapter = MoveTemp(Adapter);
    MatchmakingRegistration = FGuid::NewGuid();
    MatchmakingPlayerId = GetAuthenticatedMatchmakingPlayerId();
    if (const auto* Online = GetLocalPlayer() && GetLocalPlayer()->GetGameInstance() ?
        GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr)
        MatchmakingAuthGeneration = Online->GetSnapshot().AuthGeneration;
    OnMatchmakingCommandChanged.Broadcast();
    return MatchmakingRegistration;
}

void UDivineBeastsArenaUIClientSubsystem::UnregisterMatchmakingAdapter(FGuid Registration)
{
    if (!IsInGameThread() || !Registration.IsValid() || Registration != MatchmakingRegistration) return;
    ResetMatchmakingBridge();
}

void UDivineBeastsArenaUIClientSubsystem::ResetMatchmakingBridge()
{
    ++MatchmakingCommandGeneration;
    bMatchmakingCommandPending = false;
    MatchmakingAdapter.Reset();
    MatchmakingRegistration.Invalidate();
    MatchmakingLogicalRequestId.Empty();
    MatchmakingPlayerId.Empty();
    MatchmakingAuthGeneration.Invalidate();
    MatchmakingPartyId.Empty();
    MatchmakingMenuMode=NAME_None;
    bMatchmakingMenuRequested=false;bMatchmakingMenuDismissed=false;LastChallengeNotificationId.Empty();
    MatchmakingCommandFeedback = FText::GetEmpty();
    if(!bDeinitializing)SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Idle);
    OnMatchmakingCommandChanged.Broadcast();
}

FName UDivineBeastsArenaUIClientSubsystem::GetSelectedMatchmakingMode() const
{
    if(const auto* Source=MatchmakingSource.Get())
    {
        const FName Observed=Source->GetObservedMatchmakingMode();
        if(!Observed.IsNone())return Observed;
    }
    return MatchmakingMenuMode;
}
bool UDivineBeastsArenaUIClientSubsystem::SelectMatchmakingMenuMode(FName ModeId)
{
    if(bDeinitializing||(!ModeId.IsNone()&&!FDivineBeastsArenaModeCatalog::Find(ModeId)))return false;
    const auto State=MatchmakingAdapter?MatchmakingAdapter->GetObservedState():EGamePlatformArenaClientFlowState::Idle;
    const auto* Source=MatchmakingSource.Get();
    const bool bLocked=bMatchmakingCommandPending||!MatchmakingLogicalRequestId.IsEmpty()||
        (Source&&!Source->GetObservedMatchmakingMode().IsNone())||
        (State!=EGamePlatformArenaClientFlowState::Idle&&State!=EGamePlatformArenaClientFlowState::Failed);
    if(bLocked)
    {
        if(ModeId==GetSelectedMatchmakingMode())return true;
        MatchmakingCommandFeedback=FText::FromString(TEXT("当前模式已经锁定；切换前须确认旧邀请或匹配已取消"));
        OnMatchmakingCommandChanged.Broadcast();return false;
    }
    MatchmakingMenuMode=ModeId;OnMatchmakingCommandChanged.Broadcast();return true;
}

bool UDivineBeastsArenaUIClientSubsystem::CanRequestMatchmaking() const
{
    return !bDeinitializing && !bMatchmakingCommandPending && MatchmakingAdapter &&
        MatchmakingAdapter->SupportsTicketLifecycle() && MatchmakingAdapter->GetObservedState() == EGamePlatformArenaClientFlowState::Idle &&
        MatchmakingLogicalRequestId.IsEmpty() && IsMatchmakingAuthenticationCurrent();
}

bool UDivineBeastsArenaUIClientSubsystem::CanCancelMatchmaking() const
{
    if(bDeinitializing||bMatchmakingCommandPending||!MatchmakingAdapter||!MatchmakingAdapter->SupportsTicketLifecycle()||!IsMatchmakingAuthenticationCurrent())return false;
    if(auto* Source=MatchmakingSource.Get())if(Source->GetMatchmakingAdapter()==MatchmakingAdapter)return Source->CanCancelObservedQueue();
    return MatchmakingAdapter->GetObservedState()==EGamePlatformArenaClientFlowState::Matchmaking&&!MatchmakingLogicalRequestId.IsEmpty();
}

bool UDivineBeastsArenaUIClientSubsystem::CanRespondToMatchOffer() const
{
    if (bDeinitializing || bMatchmakingCommandPending || !MatchmakingAdapter || !MatchmakingAdapter->SupportsTicketLifecycle() ||
        !IsMatchmakingAuthenticationCurrent()) return false;
    const auto Offer = MatchmakingAdapter->GetObservedOffer();
    // 本地时间不授予确认权限；截止时间由适配器依据服务器快照判断，服务器仍重新核验版本与到期。
    return MatchmakingAdapter->GetObservedState() == EGamePlatformArenaClientFlowState::MatchFound &&
        Offer.bCanRespond && !Offer.CandidateId.IsEmpty() && Offer.Revision >= 0 && Offer.DeadlineUtc.GetTicks() > 0;
}

bool UDivineBeastsArenaUIClientSubsystem::RequestMatchmaking(FName ModeId)
{
    if (!CanRequestMatchmaking())
    {
        MatchmakingCommandFeedback = FText::FromString(TEXT("匹配状态尚未就绪，请等待同步后再试"));
        OnMatchmakingCommandChanged.Broadcast();
        return false;
    }
    auto* Instance = GetLocalPlayer()->GetGameInstance();
    const auto* Client = Instance ? Instance->GetSubsystem<UDivineBeastsArenaClientSubsystem>() : nullptr;
    FGamePlatformArenaMatchmakingRequest Request;
    FString Error;
    const FString LogicalId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    if (!Client || !Client->IsApplicationFlowInWorld() || !Client->BuildMatchmakingRequest(ModeId, MatchmakingPartyId, FString(), LogicalId, Request, Error))
    {
        MatchmakingCommandFeedback = FText::FromString(TEXT("当前模式、名单或竞技场配置不可用，未报名"));
        OnMatchmakingCommandChanged.Broadcast();
        return false;
    }
    bMatchmakingCommandPending = true;
    MatchmakingMenuMode=ModeId;
    MatchmakingLogicalRequestId = LogicalId;
    const uint64 Generation = ++MatchmakingCommandGeneration;
    const auto Adapter = MatchmakingAdapter; // 保持同步回调/提供者撤销期间对象有效。
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> Weak(this);
    const TWeakObjectPtr<UWorld> World(GetWorld());
    OnMatchmakingCommandChanged.Broadcast();
    if (MatchmakingCommandGeneration != Generation || !bMatchmakingCommandPending) return false;
    Adapter->RequestMatchmaking(Request, [Weak, World, Generation](bool bAccepted, const FString& Error)
    {
        auto* Self = Weak.Get();
        if (!IsInGameThread() || !Self || Self->bDeinitializing || !World.IsValid() || Self->GetWorld() != World.Get() ||
            Self->MatchmakingCommandGeneration != Generation || !Self->bMatchmakingCommandPending ||
            !Self->IsMatchmakingAuthenticationCurrent()) return;
        Self->bMatchmakingCommandPending = false;
        ++Self->MatchmakingCommandGeneration; // 防止错误适配器重复终态回调。
        Self->MatchmakingCommandFeedback = FText::FromString(bAccepted ? TEXT("服务器已受理报名") : TEXT("报名未获服务器确认，请查询远端状态后再操作"));
        if (bAccepted) Self->SetObservedArenaClientFlowState(Self->MatchmakingAdapter->GetObservedState());
        else if(Error==TEXT("Matchmaking.RequestRejected"))Self->MatchmakingLogicalRequestId.Empty();
        Self->OnMatchmakingCommandChanged.Broadcast();
    });
    return true;
}

bool UDivineBeastsArenaUIClientSubsystem::CancelMatchmaking()
{
    if (!CanCancelMatchmaking())
    {
        MatchmakingCommandFeedback = FText::FromString(TEXT("当前没有可确认取消的匹配合同；关闭页面或本地等待不代表退出队列"));
        OnMatchmakingCommandChanged.Broadcast();
        return false;
    }
    bMatchmakingCommandPending = true;
    const uint64 Generation = ++MatchmakingCommandGeneration;
    const auto Adapter = MatchmakingAdapter;
    const auto LogicalId = MatchmakingLogicalRequestId;
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> Weak(this);
    OnMatchmakingCommandChanged.Broadcast();
    if (MatchmakingCommandGeneration != Generation || !bMatchmakingCommandPending) return false;
    Adapter->CancelMatchmaking(LogicalId, [Weak, Generation](bool bCancelled, const FString&)
    {
        auto* Self = Weak.Get();
        if (!IsInGameThread() || !Self || Self->bDeinitializing || Self->MatchmakingCommandGeneration != Generation ||
            !Self->bMatchmakingCommandPending || !Self->IsMatchmakingAuthenticationCurrent()) return;
        Self->bMatchmakingCommandPending = false;
        ++Self->MatchmakingCommandGeneration;
        Self->MatchmakingCommandFeedback = FText::FromString(bCancelled ? TEXT("服务器已确认取消") : TEXT("取消未获确认，仍须按服务器队列状态处理"));
        if (bCancelled) { Self->MatchmakingLogicalRequestId.Empty(); Self->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Idle); }
        Self->OnMatchmakingCommandChanged.Broadcast();
    });
    return true;
}

bool UDivineBeastsArenaUIClientSubsystem::RespondToMatchOffer(bool bAccept)
{
    if (!CanRespondToMatchOffer())
    {
        MatchmakingCommandFeedback = FText::FromString(TEXT("没有可回复的授权候选身份和版本；未提交逐人确认"));
        OnMatchmakingCommandChanged.Broadcast();
        return false;
    }
    bMatchmakingCommandPending = true;
    const uint64 Generation = ++MatchmakingCommandGeneration;
    const auto Adapter = MatchmakingAdapter;
    const auto Offer = Adapter->GetObservedOffer();
    const FString LogicalId=MatchmakingLogicalRequestId;
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> Weak(this);
    OnMatchmakingCommandChanged.Broadcast();
    if (MatchmakingCommandGeneration != Generation || !bMatchmakingCommandPending) return false;
    Adapter->RespondToMatchOffer(Offer, bAccept, FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),
        [Weak, Generation, LogicalId](bool bConfirmed, const FString&)
    {
        auto* Self = Weak.Get();
        if (!IsInGameThread() || !Self || Self->bDeinitializing || Self->MatchmakingCommandGeneration != Generation ||
            !Self->bMatchmakingCommandPending || !Self->IsMatchmakingAuthenticationCurrent()) return;
        Self->bMatchmakingCommandPending = false;
        ++Self->MatchmakingCommandGeneration;
        Self->MatchmakingCommandFeedback = FText::FromString(bConfirmed ? TEXT("服务器已确认回复；等待既有流程处理新准入") : TEXT("回复未获确认，不进入竞技场"));
        if (bConfirmed)
        {
            const auto State=Self->MatchmakingAdapter->GetObservedState();
            Self->CompleteConfirmedTicketState(LogicalId,State);
            Self->SetObservedArenaClientFlowState(State);
        }
        Self->OnMatchmakingCommandChanged.Broadcast();
    });
    return true;
}

void UDivineBeastsArenaUIClientSubsystem::CompleteConfirmedTicketState(const FString& ExpectedLogicalRequestId,EGamePlatformArenaClientFlowState ConfirmedState)
{
    if(!ExpectedLogicalRequestId.IsEmpty()&&ExpectedLogicalRequestId==MatchmakingLogicalRequestId&&ConfirmedState==EGamePlatformArenaClientFlowState::Idle)
        MatchmakingLogicalRequestId.Empty();
}

bool UDivineBeastsArenaUIClientSubsystem::CanSendChallenge(FName ModeId) const
{
    FString Reason;const auto* Source=MatchmakingSource.Get();
    return CanRequestMatchmaking()&&Source&&Source->GetMatchmakingAdapter()==MatchmakingAdapter&&Source->CanChallengeMode(ModeId,Reason);
}
bool UDivineBeastsArenaUIClientSubsystem::CanReplyChallenge() const
{
    const auto* Source=MatchmakingSource.Get();
    return !bDeinitializing&&!bMatchmakingCommandPending&&IsMatchmakingAuthenticationCurrent()&&Source&&
        Source->GetMatchmakingAdapter()==MatchmakingAdapter&&Source->IsApplicationFlowInWorld()&&Source->GetObservedChallenge().bCanReply;
}
bool UDivineBeastsArenaUIClientSubsystem::CanCancelChallengeInvitation() const
{
    const auto* Source=MatchmakingSource.Get();
    return !bDeinitializing&&!bMatchmakingCommandPending&&IsMatchmakingAuthenticationCurrent()&&Source&&
        Source->GetMatchmakingAdapter()==MatchmakingAdapter&&Source->IsApplicationFlowInWorld()&&Source->GetObservedChallenge().bCanCancel;
}
bool UDivineBeastsArenaUIClientSubsystem::SendChallenge(FName ModeId,const FString& TargetPlayerId)
{return SubmitChallengeCommand(TEXT("challenge"),ModeId,TargetPlayerId,false);}
bool UDivineBeastsArenaUIClientSubsystem::ReplyChallenge(bool bAccept)
{return SubmitChallengeCommand(TEXT("challengeReply"),NAME_None,FString(),bAccept);}
bool UDivineBeastsArenaUIClientSubsystem::CancelChallengeInvitation()
{return SubmitChallengeCommand(TEXT("challengeCancel"),NAME_None,FString(),false);}
bool UDivineBeastsArenaUIClientSubsystem::SubmitChallengeCommand(FName Action,FName ModeId,const FString& TargetPlayerId,bool bAccept)
{
    const bool bSend=Action==TEXT("challenge"),bReply=Action==TEXT("challengeReply");
    bool bTargetValid=!TargetPlayerId.IsEmpty()&&TargetPlayerId.Len()<=128&&TargetPlayerId.TrimStartAndEnd()==TargetPlayerId;
    for(const TCHAR Character:TargetPlayerId)if(Character<32||Character==127)bTargetValid=false;
    if((bSend&&(!CanSendChallenge(ModeId)||!bTargetValid||TargetPlayerId==MatchmakingPlayerId))||
        (bReply&&!CanReplyChallenge())||(!bSend&&!bReply&&(Action!=TEXT("challengeCancel")||!CanCancelChallengeInvitation())))
    {
        MatchmakingCommandFeedback=FText::FromString(TEXT("约战目标、完整队伍或邀请状态尚不可用，请等待同步并检查公开玩家ID"));
        OnMatchmakingCommandChanged.Broadcast();return false;
    }
    auto* Source=MatchmakingSource.Get();
    bMatchmakingCommandPending=true;
    if(bSend)MatchmakingMenuMode=ModeId;
    const FString Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    if(MatchmakingLogicalRequestId.IsEmpty())MatchmakingLogicalRequestId=Id;
    const FString LogicalId=MatchmakingLogicalRequestId;
    const uint64 Generation=++MatchmakingCommandGeneration;
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> Weak(this);
    const TWeakObjectPtr<UWorld> World(GetWorld());
    OnMatchmakingCommandChanged.Broadcast();
    if(bDeinitializing||MatchmakingCommandGeneration!=Generation||!bMatchmakingCommandPending||MatchmakingSource.Get()!=Source)return false;
    auto Completion=[Weak,World,Generation,LogicalId](bool bConfirmed,const FString& Error)
    {
        auto* Self=Weak.Get();
        if(!IsInGameThread()||!Self||Self->bDeinitializing||!World.IsValid()||Self->GetWorld()!=World.Get()||
            Self->MatchmakingCommandGeneration!=Generation||!Self->bMatchmakingCommandPending||!Self->IsMatchmakingAuthenticationCurrent())return;
        Self->bMatchmakingCommandPending=false;++Self->MatchmakingCommandGeneration;
        Self->MatchmakingCommandFeedback=FText::FromString(bConfirmed?TEXT("服务器已确认约战操作；邀请接受后仍须全员分别确认"):
            Error==TEXT("Matchmaking.CommandUnconfirmed")?TEXT("约战结果未知，正在重新同步；不会重复发起邀请"):TEXT("约战未获受理；对方可能忙碌、名单变化或邀请已结束"));
        if(Self->MatchmakingAdapter)
        {
            const auto State=Self->MatchmakingAdapter->GetObservedState();
            Self->CompleteConfirmedTicketState(LogicalId,State);
            if(bConfirmed)Self->SetObservedArenaClientFlowState(State);
        }
        Self->OnMatchmakingCommandChanged.Broadcast();
    };
    if(bSend)Source->RequestChallenge(ModeId,TargetPlayerId,Id,MoveTemp(Completion));
    else if(bReply)Source->ReplyChallenge(bAccept,Id,MoveTemp(Completion));
    else Source->CancelChallenge(Id,MoveTemp(Completion));
    return true;
}

void UDivineBeastsArenaUIClientSubsystem::RefreshMatchmakingSource()
{
    if(bDeinitializing)return;auto* Source=MatchmakingSource.Get();if(!Source)return;
    if(GetAuthenticatedMatchmakingPlayerId().IsEmpty()){if(MatchmakingAdapter)ResetMatchmakingBridge();return;}
    const auto Adapter=Source->GetMatchmakingAdapter();if(!Adapter)return;
    if(!MatchmakingAdapter)RegisterMatchmakingAdapter(Adapter);
    if(MatchmakingAdapter!=Adapter)return; // 独立测试/显式工具提供者拥有其注册，真实来源不越权替换。
    MatchmakingCommandFeedback=Source->GetObservedMatchmakingStatus();
    const auto Challenge=Source->GetObservedChallenge();
    if(Challenge.bPresent&&LastChallengeNotificationId!=Challenge.ChallengeId)
    {LastChallengeNotificationId=Challenge.ChallengeId;bMatchmakingMenuRequested=true;bMatchmakingMenuDismissed=false;}
    const FName ObservedMode=Source->GetObservedMatchmakingMode();
    if(!ObservedMode.IsNone())MatchmakingMenuMode=ObservedMode;
    if(Adapter->SupportsTicketLifecycle())
    {
        const auto State=Adapter->GetObservedState();
        if(State==EGamePlatformArenaClientFlowState::Idle)MatchmakingLogicalRequestId.Empty();
        SetObservedArenaClientFlowState(State);
    }
    OnMatchmakingCommandChanged.Broadcast();
}
