// 项目竞技本地玩家命令桥：把页面意图交给MOBA公开适配端口，绝不HTTP、直接切图或计算队列。
// 提供者须由竞技组合根按真实后端能力注入；当前Gateway缺失观察/取消/候选确认，不注册伪生产实现。
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "Client/GamePlatformArenaMatchmakingAdapter.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "ViewModels/GamePlatformArenaViewModel.h"

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
    });
}

void UDivineBeastsArenaUIClientSubsystem::DisconnectMatchmakingAuthentication()
{
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
    MatchmakingCommandFeedback = FText::GetEmpty();
    OnMatchmakingCommandChanged.Broadcast();
}

bool UDivineBeastsArenaUIClientSubsystem::CanRequestMatchmaking() const
{
    return !bDeinitializing && !bMatchmakingCommandPending && MatchmakingAdapter &&
        MatchmakingAdapter->SupportsTicketLifecycle() && MatchmakingAdapter->GetObservedState() == EGamePlatformArenaClientFlowState::Idle &&
        MatchmakingLogicalRequestId.IsEmpty() && IsMatchmakingAuthenticationCurrent();
}

bool UDivineBeastsArenaUIClientSubsystem::CanCancelMatchmaking() const
{
    return !bDeinitializing && !bMatchmakingCommandPending && MatchmakingAdapter && MatchmakingAdapter->SupportsTicketLifecycle() &&
        MatchmakingAdapter->GetObservedState() == EGamePlatformArenaClientFlowState::Matchmaking &&
        !MatchmakingLogicalRequestId.IsEmpty() && IsMatchmakingAuthenticationCurrent();
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
        MatchmakingCommandFeedback = FText::FromString(TEXT("真实匹配后端的观察、取消和逐人确认合同尚未完整接入，未创建队列票据"));
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
        MatchmakingCommandFeedback = FText::FromString(TEXT("当前流程或所选模式缺少正式地图、规则与准入配置，未报名"));
        OnMatchmakingCommandChanged.Broadcast();
        return false;
    }
    bMatchmakingCommandPending = true;
    MatchmakingLogicalRequestId = LogicalId;
    const uint64 Generation = ++MatchmakingCommandGeneration;
    const auto Adapter = MatchmakingAdapter; // 保持同步回调/提供者撤销期间对象有效。
    const TWeakObjectPtr<UDivineBeastsArenaUIClientSubsystem> Weak(this);
    const TWeakObjectPtr<UWorld> World(GetWorld());
    OnMatchmakingCommandChanged.Broadcast();
    if (MatchmakingCommandGeneration != Generation || !bMatchmakingCommandPending) return false;
    Adapter->RequestMatchmaking(Request, [Weak, World, Generation](bool bAccepted, const FString&)
    {
        auto* Self = Weak.Get();
        if (!IsInGameThread() || !Self || Self->bDeinitializing || !World.IsValid() || Self->GetWorld() != World.Get() ||
            Self->MatchmakingCommandGeneration != Generation || !Self->bMatchmakingCommandPending ||
            !Self->IsMatchmakingAuthenticationCurrent()) return;
        Self->bMatchmakingCommandPending = false;
        ++Self->MatchmakingCommandGeneration; // 防止错误适配器重复终态回调。
        Self->MatchmakingCommandFeedback = FText::FromString(bAccepted ? TEXT("服务器已受理报名") : TEXT("报名未获服务器确认，请查询远端状态后再操作"));
        if (bAccepted) Self->SetObservedArenaClientFlowState(Self->MatchmakingAdapter->GetObservedState());
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
