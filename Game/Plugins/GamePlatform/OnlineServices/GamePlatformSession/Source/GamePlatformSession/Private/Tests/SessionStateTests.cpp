#if defined(GAMEPLATFORM_SESSION_NATIVE_TEST)
#include "../State/SessionConnectionState.h"
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace GamePlatformSession;

// 测试直接调用生产状态内核；模拟事实仅验证状态规则，绝不表示UE网络已连接。
static void Require(bool Condition, const char* Message)
{
    if (!Condition) throw std::runtime_error(Message);
}

static FBinding Binding(std::uint64_t Epoch = 1)
{
    return { "assignment-" + std::to_string(Epoch), "game-session", "server-" + std::to_string(Epoch), "boot", "world", "1", Epoch };
}

static FOperationIdentity Begin(FSessionConnectionState& State, EIntent Intent, const std::string& Name)
{
    FOperationIdentity Identity;
    Require(State.Begin(Intent, Name, "attempt-" + Name, 0, 30, Identity) == EAcceptance::Accepted, "begin");
    return Identity;
}

static void Connect(FSessionConnectionState& State, const FOperationIdentity& Identity, const FBinding& Target)
{
    Require(State.Assign(Identity, Target, 1) == EAcceptance::Accepted, "assign");
    Require(State.CommitTravel(Identity, 2) == EAcceptance::Accepted, "travel");
    for (auto Fact : { EFact::AdmissionConfirmed, EFact::TargetWorldLoaded, EFact::ControllerReady })
        Require(State.Observe(Identity, Target, Fact, 3) == EAcceptance::Accepted, "fact");
    Require(State.Snapshot().State != EState::Ready, "three facts cannot be ready");
    Require(State.Observe(Identity, Target, EFact::NetworkConnected, 4) == EAcceptance::Accepted, "network fact");
    Require(State.Snapshot().State == EState::Ready, "four facts ready");
}

int main()
{
    try
    {
        FSessionConnectionState State("scope-a");
        FOperationIdentity Identity;
        Require(State.Begin(EIntent::Join, "unauth", "attempt", 0, 30, Identity) == EAcceptance::NotAuthenticated, "reject unauthenticated");
        Require(State.SetAuthentication("auth-a", 1) == EAcceptance::Accepted, "auth");
        Identity = Begin(State, EIntent::Join, "join");
        FOperationIdentity Duplicate;
        Require(State.Begin(EIntent::Join, "join", "attempt-join", 0, 50, Duplicate) == EAcceptance::Accepted && Duplicate == Identity, "idempotent active operation");
        Require(State.Begin(EIntent::Join, "other", "other", 0, 30, Duplicate) == EAcceptance::Busy, "busy");
        Require(State.Observe(Identity, Binding(), EFact::NetworkConnected, 1) == EAcceptance::Invalid, "fact before travel");
        Connect(State, Identity, Binding());
        const auto FinishedCount = State.Snapshot().CompletionCount;
        Require(State.Observe(Identity, Binding(), EFact::NetworkConnected, 5) == EAcceptance::Stale, "terminal once");
        Require(State.Snapshot().CompletionCount == FinishedCount, "no second completion");
        auto Copy = State.Snapshot();
        Copy.Current.ServerInstanceId = "mutated";
        Require(State.Snapshot().Current.ServerInstanceId != "mutated", "snapshot value isolation");
        const auto Transfer = Begin(State, EIntent::Transfer, "transfer");
        Require(State.Assign(Transfer, Binding(2), 1) == EAcceptance::Accepted, "transfer pending");
        Require(State.Snapshot().Current == Binding(), "source retained during preparation");
        Require(State.Cancel(Transfer, 2) == EAcceptance::Accepted, "pretravel cancel");
        Require(State.Snapshot().Current == Binding() && State.Snapshot().bRemoteResolutionRequired, "cancel preserves source and requires remote resolution");
        Require(State.ResolveRemote(Transfer, Binding()) == EAcceptance::Accepted, "source confirmed");
        const auto Next = Begin(State, EIntent::Transfer, "next");
        Require(State.Assign(Next, Binding(2), 1) == EAcceptance::Accepted, "next target");
        Require(State.CommitTravel(Next, 2) == EAcceptance::Accepted, "next travel");
        Require(State.Observe(Next, Binding(), EFact::AdmissionConfirmed, 3) == EAcceptance::Invalid, "wrong instance rejected");
        Require(State.Cancel(Next, 4) == EAcceptance::Accepted, "posttravel cancel");
        Require(State.Snapshot().LastOutcome == EOutcome::Uncertain && !State.Snapshot().Current.IsValid(), "cannot pretend rollback");
        Require(State.ResolveRemote(Next, Binding(2)) == EAcceptance::Invalid, "backend binding cannot fabricate network ready");
        Require(State.ResolveRemote(Next, {}) == EAcceptance::Accepted, "released confirmed");
        const auto Reconnect = Begin(State, EIntent::Reconnect, "reconnect");
        Connect(State, Reconnect, Binding(2));
        Require(!State.Disconnect(Binding()), "old source logout fenced");
        State.Leave();
        Require(State.Snapshot().State == EState::Idle && State.Snapshot().bRemoteResolutionRequired, "leave local and remote distinct");
        Require(State.ResolveRemote(Reconnect, {}) == EAcceptance::Accepted, "leave confirmed");
        const auto Timeout = Begin(State, EIntent::Join, "timeout");
        State.AdvanceDeadline(30);
        Require(State.Snapshot().LastOutcome == EOutcome::TimedOut, "monotonic deadline");
        Require(State.Assign(Timeout, Binding(), 31) == EAcceptance::Stale, "late target ignored");
        Require(State.Snapshot().bRemoteResolutionRequired, "lost allocation response remains uncertain");
        Require(State.ResolveRemote(Timeout, {}) == EAcceptance::Accepted, "timeout resolved");
        const auto OldAccount = Begin(State, EIntent::Join, "old-auth");
        Require(State.SetAuthentication("auth-b", 2) == EAcceptance::Accepted, "switch account");
        Require(State.Assign(OldAccount, Binding(), 1) == EAcceptance::Stale, "old account callback fenced");
        Require(State.SetAuthentication("auth-c", 2) == EAcceptance::Stale, "same generation different account");
        Require(State.Begin(EIntent::Join, "nan", "nan", std::numeric_limits<double>::quiet_NaN(), 30, Identity) == EAcceptance::Invalid, "invalid clock");
        FSessionConnectionState Other("scope-b");
        Other.SetAuthentication("auth-a", 1);
        const auto OtherIdentity = Begin(Other, EIntent::Join, "independent");
        Require(Other.Assign(OldAccount, Binding(), 1) == EAcceptance::Stale, "cross instance token fenced");
        Connect(Other, OtherIdentity, Binding());
        Require(Other.Snapshot().State == EState::Ready && State.Snapshot().State == EState::Idle, "independent scopes");
        std::cout << "Session state policy assertions passed; no UE network exercised.\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << Error.what() << '\n';
        return 1;
    }
}
#endif
