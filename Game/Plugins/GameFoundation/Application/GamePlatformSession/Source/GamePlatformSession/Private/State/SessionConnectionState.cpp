#include "SessionConnectionState.h"
#include <cmath>
#include <utility>

namespace GamePlatformSession
{
bool FOperationIdentity::operator==(const FOperationIdentity& Other) const
{
    return ScopeId == Other.ScopeId && OperationId == Other.OperationId && AttemptId == Other.AttemptId
        && AuthGeneration == Other.AuthGeneration && ConnectionGeneration == Other.ConnectionGeneration;
}

bool FBinding::IsValid() const
{
    return !AssignmentId.empty() && !GameSessionId.empty() && !ServerInstanceId.empty()
        && !ServerBootId.empty() && !WorldId.empty() && !ProtocolVersion.empty() && SessionEpoch > 0;
}

bool FBinding::operator==(const FBinding& Other) const
{
    return AssignmentId == Other.AssignmentId && GameSessionId == Other.GameSessionId
        && ServerInstanceId == Other.ServerInstanceId && ServerBootId == Other.ServerBootId
        && WorldId == Other.WorldId && ProtocolVersion == Other.ProtocolVersion && SessionEpoch == Other.SessionEpoch;
}

FSessionConnectionState::FSessionConnectionState(std::string ScopeId) : Scope(std::move(ScopeId)) {}

EAcceptance FSessionConnectionState::SetAuthentication(std::string AuthSessionId, std::uint64_t Generation)
{
    if (Generation == 0 || Generation < AuthGeneration || (Generation == AuthGeneration && AuthSessionId != AuthSession))
        return EAcceptance::Stale;
    if (Generation == AuthGeneration) return EAcceptance::Accepted;
    if (View.bOperationActive) Finish(EOutcome::AuthChanged, false, true);
    View.Current = {};
    View.Pending = {};
    View.State = EState::Idle;
    AuthSession = std::move(AuthSessionId);
    AuthGeneration = Generation;
    View.bRemoteResolutionRequired = false;
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::Begin(EIntent Intent, std::string OperationId, std::string AttemptId,
    double NowSeconds, double DeadlineSeconds, FOperationIdentity& OutIdentity)
{
    OutIdentity = {};
    AdvanceDeadline(NowSeconds);
    if (Scope.empty() || OperationId.empty() || AttemptId.empty() || !std::isfinite(NowSeconds)
        || !std::isfinite(DeadlineSeconds) || DeadlineSeconds <= NowSeconds) return EAcceptance::Invalid;
    if (AuthSession.empty()) return EAcceptance::NotAuthenticated;
    if (View.bOperationActive)
    {
        if (View.Operation.OperationId == OperationId && View.Operation.AttemptId == AttemptId && ActiveIntent == Intent)
        {
            OutIdentity = View.Operation;
            return EAcceptance::Accepted;
        }
        return EAcceptance::Busy;
    }
    if (View.bRemoteResolutionRequired) return EAcceptance::WrongState;
    if ((Intent == EIntent::Join && View.Current.IsValid()) || (Intent == EIntent::Transfer && !View.Current.IsValid()))
        return EAcceptance::WrongState;
    View.Operation = { Scope, std::move(OperationId), std::move(AttemptId), AuthGeneration, ++ConnectionGeneration };
    OutIdentity = View.Operation;
    ActiveIntent = Intent;
    View.bOperationActive = true;
    View.LastOutcome = EOutcome::None;
    View.Pending = {};
    View.State = Intent == EIntent::Transfer ? EState::Transferring : Intent == EIntent::Reconnect ? EState::Reconnecting : EState::RequestingAssignment;
    Deadline = DeadlineSeconds;
    Facts = 0;
    bTravelCommitted = false;
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::Check(const FOperationIdentity& Identity, double NowSeconds)
{
    if (!std::isfinite(NowSeconds)) return EAcceptance::Invalid;
    AdvanceDeadline(NowSeconds);
    if (!View.bOperationActive || !(Identity == View.Operation)) return EAcceptance::Stale;
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::Assign(const FOperationIdentity& Identity, const FBinding& Binding, double NowSeconds)
{
    const auto Checked = Check(Identity, NowSeconds);
    if (Checked != EAcceptance::Accepted) return Checked;
    if (!Binding.IsValid()) return EAcceptance::Invalid;
    if (View.Pending.IsValid()) return View.Pending == Binding ? EAcceptance::Accepted : EAcceptance::WrongState;
    if (View.Current.IsValid() && Binding.SessionEpoch <= View.Current.SessionEpoch) return EAcceptance::Invalid;
    View.Pending = Binding;
    View.State = EState::PreparingConnection;
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::CommitTravel(const FOperationIdentity& Identity, double NowSeconds)
{
    const auto Checked = Check(Identity, NowSeconds);
    if (Checked != EAcceptance::Accepted) return Checked;
    if (!View.Pending.IsValid()) return EAcceptance::WrongState;
    if (bTravelCommitted) return EAcceptance::Accepted;
    bTravelCommitted = true;
    View.Current = {};
    View.State = EState::Connecting;
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::Observe(const FOperationIdentity& Identity, const FBinding& Binding, EFact Fact, double NowSeconds)
{
    const auto Checked = Check(Identity, NowSeconds);
    if (Checked != EAcceptance::Accepted) return Checked;
    if (!bTravelCommitted || !Binding.IsValid() || !(Binding == View.Pending)) return EAcceptance::Invalid;
    const auto Index = static_cast<unsigned int>(Fact);
    if (Index > static_cast<unsigned int>(EFact::ControllerReady)) return EAcceptance::Invalid;
    Facts |= 1u << Index;
    View.State = EState::AwaitingAdmission;
    if (Facts == 15u)
    {
        View.Current = View.Pending;
        Finish(EOutcome::Succeeded, true, false);
    }
    return EAcceptance::Accepted;
}

void FSessionConnectionState::Finish(EOutcome Outcome, bool bKeepSource, bool bNeedsResolution)
{
    if (!View.bOperationActive) return;
    View.bOperationActive = false;
    View.LastOutcome = Outcome;
    ++View.CompletionCount;
    if (!bKeepSource) View.Current = {};
    View.Pending = {};
    View.State = View.Current.IsValid() ? EState::Ready : EState::Idle;
    View.bRemoteResolutionRequired = bNeedsResolution;
    Facts = 0;
}

EAcceptance FSessionConnectionState::Cancel(const FOperationIdentity& Identity, double NowSeconds)
{
    const auto Checked = Check(Identity, NowSeconds);
    if (Checked != EAcceptance::Accepted) return Checked;
    // 分配响应尚未到达也可能已经在远端占位，不能把缺少Pending当作已回滚。
    Finish(bTravelCommitted ? EOutcome::Uncertain : EOutcome::Cancelled, !bTravelCommitted, true);
    return EAcceptance::Accepted;
}

EAcceptance FSessionConnectionState::Fail(const FOperationIdentity& Identity, double NowSeconds)
{
    const auto Checked = Check(Identity, NowSeconds);
    if (Checked != EAcceptance::Accepted) return Checked;
    Finish(bTravelCommitted ? EOutcome::Uncertain : EOutcome::Failed, !bTravelCommitted, true);
    return EAcceptance::Accepted;
}

void FSessionConnectionState::AdvanceDeadline(double NowSeconds)
{
    if (View.bOperationActive && std::isfinite(NowSeconds) && NowSeconds >= Deadline)
        Finish(EOutcome::TimedOut, !bTravelCommitted, true);
}

EAcceptance FSessionConnectionState::ResolveRemote(const FOperationIdentity& Identity, const FBinding& ConfirmedBinding)
{
    if (View.bOperationActive || !(Identity == View.Operation) || Identity.AuthGeneration != AuthGeneration)
        return EAcceptance::Stale;
    if (!View.bRemoteResolutionRequired) return EAcceptance::WrongState;
    // 远端绑定存在不证明本地网络连接存在；只能保留原来未断开的同一绑定。
    if (ConfirmedBinding.IsValid() && (!View.Current.IsValid() || !(ConfirmedBinding == View.Current)))
        return EAcceptance::Invalid;
    if (!ConfirmedBinding.IsValid()) View.Current = {};
    View.State = View.Current.IsValid() ? EState::Ready : EState::Idle;
    View.bRemoteResolutionRequired = false;
    return EAcceptance::Accepted;
}

bool FSessionConnectionState::Disconnect(const FBinding& Binding)
{
    if (!View.Current.IsValid() || !(View.Current == Binding)) return false;
    View.Current = {};
    if (!View.bOperationActive) View.State = EState::Idle;
    return true;
}

void FSessionConnectionState::Leave()
{
    const bool HadRemoteWork = View.Current.IsValid() || View.Pending.IsValid() || View.bRemoteResolutionRequired;
    if (View.bOperationActive) Finish(EOutcome::Cancelled, false, HadRemoteWork);
    View.Current = {};
    View.Pending = {};
    View.State = EState::Idle;
    View.bRemoteResolutionRequired = HadRemoteWork;
}
}
