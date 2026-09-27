// 测试替身仅编译进测试：模拟传输边界，断言生产状态机输出，绝不接入正式门面。
#if defined(GAMEPLATFORM_ONLINE_NATIVE_TEST)
#include "Requests/OnlineSession.h"
#include <cstdio>
#include <stdexcept>

namespace GamePlatformOnline::Tests
{
using namespace Internal;
static void Require(bool bValue, const char* Message)
{
    if (!bValue) throw std::runtime_error(Message);
}

struct FTransport final : ITransport
{
    struct FSent { FWireRequest Request; FReceive Receive; };
    std::vector<FSent> Sent;
    std::vector<std::string> Cancelled;
    bool bStart = true;
    bool SupportsSecureRequests() const override { return true; }
    bool Send(FWireRequest Request, FReceive Receive) override
    {
        Sent.push_back({std::move(Request), std::move(Receive)});
        return bStart;
    }
    void Cancel(const std::string& Attempt) override { Cancelled.push_back(Attempt); }
    void Reply(size_t Index, FReply Reply) { Sent.at(Index).Receive(std::move(Reply)); }
};

struct FFixture
{
    FTransport Transport;
    unsigned Counter = 0;
    FSession Session{Transport, [this] { return "id-" + std::to_string(++Counter); }};
    double Now = 1.0;
    std::vector<FOutcome> Results;
    FFixture() { Require(Session.Configure(Config()) == EError::None, "valid configuration"); }
    static FConfiguration Config()
    {
        FConfiguration C;
        C.Origin = "https://gateway.example"; C.GameId = "test"; C.ClientVersion = "1";
        return C;
    }
    std::string Submit(EOperation Op, FInput Input = {}, FOptions Options = {})
    {
        return Session.Submit(Op, std::move(Input), std::move(Options),
            [this](FOutcome R) { Results.push_back(std::move(R)); }, Now, 1000.0);
    }
    void Tick() { Session.Tick(Now, 1000.0); }
    static FReply Auth(std::string Access = "access-1", std::string Refresh = "refresh-1")
    {
        FReply R; R.Error = EError::None;
        R.Auth = {"player-a", "session-a", std::move(Access), std::move(Refresh), 2000.0, 3000.0};
        return R;
    }
    static FReply Profile(std::int64_t Revision)
    {
        FReply R; R.Error = EError::None;
        R.Profile.PlayerId = "player-a"; R.Profile.GameId = "test";
        R.Profile.DisplayName = "Player"; R.Profile.Revision = Revision; R.Profile.DataVersion = 1;
        return R;
    }
    static FReply Error(EError E) { FReply R; R.Error = E; return R; }
    void Login()
    {
        FInput I; I.AccountName = "a"; I.Credential = "password";
        Submit(EOperation::Login, std::move(I)); Tick();
        Transport.Reply(Transport.Sent.size() - 1, Auth()); Tick(); Tick();
        Require(Session.Authentication().State == EAuthState::SignedIn, "login adopts real reply");
        Results.clear();
    }
};

static void ConfigurationFailClosed()
{
    auto C = FFixture::Config();
    Require(ValidateConfiguration(C, false) == EError::None, "https valid");
    C.Origin = "http://127.0.0.1:8080";
    Require(ValidateConfiguration(C, false) == EError::InvalidConfiguration, "http explicit only");
    C.bAllowLoopbackHttp = true;
    Require(ValidateConfiguration(C, false) == EError::None, "literal loopback");
    Require(ValidateConfiguration(C, true) == EError::InvalidConfiguration, "shipping rejects http");
    for (const auto* Origin : {"http://127.0.0.1.evil", "http://localhost", "http://192.168.1.2", "https://a@b", "https://a/path", "https://a?x", "https://a:0", "https://a:65536", "https://a\\evil"})
    { C.Origin = Origin; Require(ValidateConfiguration(C, false) == EError::InvalidConfiguration, "unsafe origin rejected"); }
    C = FFixture::Config(); C.bVerifyCertificates = false;
    Require(ValidateConfiguration(C, false) == EError::InvalidConfiguration, "tls verification required");
    FBlockedTransport Blocked;
    FSession Session(Blocked, [] { return "blocked"; });
    Require(Session.Configure(FFixture::Config()) == EError::UnsupportedTransport, "production fail closed");
}

static void DeferredOnceAndFailedStart()
{
    FFixture F; F.Transport.bStart = false;
    auto Id = F.Submit(EOperation::Probe);
    Require(F.Results.empty(), "not inline"); F.Tick();
    Require(F.Results.empty(), "failure queued next tick"); F.Tick();
    Require(F.Results.size() == 1 && F.Results[0].Error == EError::ConnectionFailure, "start failure terminal");
    F.Transport.Reply(0, FFixture::Error(EError::Timeout)); F.Tick(); F.Tick();
    Require(F.Results.size() == 1 && !F.Session.Cancel(Id), "late callback and cancel once");
}

static void RefreshSingleFlightAndOldToken()
{
    FFixture F; F.Login();
    F.Submit(EOperation::ReadProfile); F.Submit(EOperation::ReadProfile); F.Tick();
    F.Transport.Reply(1, FFixture::Error(EError::Unauthenticated)); F.Tick();
    Require(F.Transport.Sent.size() == 4, "one refresh started");
    F.Transport.Reply(3, FFixture::Auth("access-2", "refresh-2")); F.Tick();
    F.Transport.Reply(2, FFixture::Error(EError::Unauthenticated)); F.Tick();
    Require(F.Session.Diagnostics().RefreshAttempts == 1, "old token 401 uses fresh token");
    Require(F.Transport.Sent.back().Request.AccessToken == "access-2", "replay new access");
}

static void ForbiddenAndLostRefresh()
{
    FFixture F; F.Login(); F.Submit(EOperation::ReadProfile); F.Tick();
    F.Transport.Reply(1, FFixture::Error(EError::Forbidden)); F.Tick(); F.Tick();
    Require(F.Session.Diagnostics().RefreshAttempts == 0 && F.Results.back().Error == EError::Forbidden, "403 never refresh");
    F.Submit(EOperation::Refresh); F.Tick();
    F.Transport.Reply(2, FFixture::Error(EError::ConnectionFailure)); F.Tick(); F.Tick();
    Require(F.Results.back().Error == EError::OutcomeUnknown, "lost rotation unknown");
    Require(F.Session.Authentication().State == EAuthState::ReauthenticationRequired, "lost rotation needs login");
    F.Now += 100; F.Tick(); Require(F.Transport.Sent.size() == 3, "no retry rotation");
}

static void LogoutRejectsLateAuthentication()
{
    FFixture F; F.Login(); F.Submit(EOperation::Refresh); F.Tick();
    F.Submit(EOperation::Logout);
    Require(F.Session.Authentication().State == EAuthState::SignedOut && !F.Session.CachedProfile(), "logout clears before return");
    F.Transport.Reply(1, FFixture::Auth("late", "late")); F.Tick(); F.Tick();
    Require(F.Session.Authentication().State == EAuthState::SignedOut, "late refresh cannot restore");
    FReply Revoke; Revoke.Error = EError::None;
    F.Transport.Reply(2, Revoke); F.Tick(); F.Tick();
    Require(F.Results.back().Logout == ELogout::ServerRevoked, "revocation explicit");
}

static void CancelWaiterAndScope()
{
    FFixture F; F.Login();
    auto A = F.Submit(EOperation::Refresh); auto B = F.Submit(EOperation::Refresh); F.Tick();
    Require(F.Transport.Sent.size() == 2, "explicit refresh merged");
    Require(F.Session.Cancel(A), "cancel waiter");
    F.Transport.Reply(1, FFixture::Auth()); F.Tick(); F.Tick();
    Require(F.Results.size() == 2 && F.Results.back().RequestId == B && F.Results.back().Error == EError::None, "other waiter survives");
    bool bAlive = true; FOptions O; O.IsAlive = [&] { return bAlive; };
    F.Submit(EOperation::ReadProfile, {}, O); F.Tick(); bAlive = false;
    F.Transport.Reply(2, FFixture::Profile(3)); F.Tick(); F.Tick();
    Require(!F.Session.CachedProfile() && F.Results.back().Error == EError::Cancelled, "dead owner no publish");
}

static void MonotonicProfileAndAccountIsolation()
{
    FFixture A; FFixture B; A.Login();
    A.Submit(EOperation::ReadProfile); A.Submit(EOperation::ReadProfile); A.Tick();
    A.Transport.Reply(2, FFixture::Profile(9007199254740993LL)); A.Tick();
    A.Transport.Reply(1, FFixture::Profile(3)); A.Tick(); A.Tick();
    Require(A.Session.CachedProfile()->Revision == 9007199254740993LL, "cache int64 monotonic");
    Require(!B.Session.CachedProfile() && B.Session.Authentication().State == EAuthState::SignedOut, "instances isolated");
    FInput I; I.AccountName = "b"; I.Credential = "secret";
    A.Submit(EOperation::Login, I); A.Tick();
    Require(A.Results.back().Error == EError::AuthenticationBusy, "explicit logout required");
}

static void BudgetsRetriesAndShutdown()
{
    FFixture F; auto C = FFixture::Config(); C.MaxConcurrent = 1; C.MaxQueued = 1; C.MaxReadRetries = 1;
    Require(F.Session.Configure(C) == EError::None, "idle reconfigure");
    F.Submit(EOperation::Probe); F.Tick(); F.Submit(EOperation::Probe); F.Submit(EOperation::Probe); F.Tick();
    Require(F.Results.back().Error == EError::QueueFull && F.Transport.Sent.size() == 1, "queue and concurrency bounded");
    F.Transport.Reply(0, FFixture::Error(EError::ServiceUnavailable)); F.Tick();
    F.Now += 1; F.Tick();
    Require(F.Transport.Sent.size() == 2, "available capacity schedules pending");
    F.Session.Shutdown(); F.Tick(); F.Tick();
    Require(F.Session.Diagnostics().Active == 0 && F.Results.size() == 3, "shutdown once drains");
}

int RunOnlineSessionTests()
{
    struct FCase { const char* Name; void (*Run)(); };
    const FCase Cases[] = {
        {"ConfigurationFailClosed", ConfigurationFailClosed}, {"DeferredOnceAndFailedStart", DeferredOnceAndFailedStart},
        {"RefreshSingleFlightAndOldToken", RefreshSingleFlightAndOldToken}, {"ForbiddenAndLostRefresh", ForbiddenAndLostRefresh},
        {"LogoutRejectsLateAuthentication", LogoutRejectsLateAuthentication}, {"CancelWaiterAndScope", CancelWaiterAndScope},
        {"MonotonicProfileAndAccountIsolation", MonotonicProfileAndAccountIsolation}, {"BudgetsRetriesAndShutdown", BudgetsRetriesAndShutdown}
    };
    int Failed = 0;
    for (const auto& C : Cases)
    {
        try { C.Run(); std::printf("PASS %s\n", C.Name); }
        catch (const std::exception& E) { ++Failed; std::printf("FAIL %s: %s\n", C.Name, E.what()); }
    }
    return Failed;
}
}
#if defined(GAMEPLATFORM_ONLINE_NATIVE_TEST)
int main() { return GamePlatformOnline::Tests::RunOnlineSessionTests(); }
#endif
#endif
