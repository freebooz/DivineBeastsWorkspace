#include "Requests/OnlineSession.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace GamePlatformOnline::Internal
{
namespace
{
bool IsAsciiIdentity(const std::string& S, size_t Max)
{
    return !S.empty() && S.size() <= Max && std::all_of(S.begin(), S.end(), [](unsigned char C) { return C >= 33 && C <= 126; });
}
bool IsOriginValid(const FConfiguration& C, bool bShipping)
{
    const bool bHttps = C.Origin.rfind("https://", 0) == 0;
    const bool bHttp = C.Origin.rfind("http://", 0) == 0;
    if ((!bHttps && !bHttp) || C.Origin.size() > 2048 || (!bHttps && (!C.bAllowLoopbackHttp || bShipping))) return false;
    std::string Authority = C.Origin.substr(bHttps ? 8 : 7);
    if (Authority.empty() || Authority.find_first_of("/@?#\\%\r\n\t ") != std::string::npos) return false;
    std::string Host, Port;
    if (Authority[0] == '[')
    {
        // 第一版只允许字面 IPv6 回环，不实现第二套通用 IPv6 解析器。
        if (Authority.rfind("[::1]", 0) != 0) return false;
        Host = "[::1]";
        if (Authority.size() > 5) { if (Authority[5] != ':') return false; Port = Authority.substr(6); if (Port.empty()) return false; }
    }
    else
    {
        const auto Colon = Authority.find(':'); Host = Authority.substr(0, Colon);
        if (Colon != std::string::npos) { Port = Authority.substr(Colon + 1); if (Port.empty()) return false; }
        if (Host.empty() || Host.front() == '.' || Host.back() == '.') return false;
        size_t Start = 0;
        while (Start < Host.size())
        {
            auto End = Host.find('.', Start); if (End == std::string::npos) End = Host.size();
            if (End == Start || End - Start > 63 || Host[Start] == '-' || Host[End - 1] == '-') return false;
            for (size_t I = Start; I < End; ++I)
                if (!((Host[I] >= 'a' && Host[I] <= 'z') || (Host[I] >= '0' && Host[I] <= '9') || Host[I] == '-')) return false;
            Start = End + 1;
        }
    }
    if (!Port.empty())
    {
        if (Port.size() > 5) return false;
        unsigned Value = 0;
        for (char D : Port) { if (D < '0' || D > '9') return false; Value = Value * 10 + static_cast<unsigned>(D - '0'); }
        if (Value == 0 || Value > 65535) return false;
    }
    return bHttps || Host == "127.0.0.1" || Host == "[::1]";
}
bool IsTransient(EError E)
{
    return E == EError::ConnectionFailure || E == EError::Timeout || E == EError::RateLimited || E == EError::ServiceUnavailable;
}
bool IsUncertain(EError E)
{
    return E == EError::ConnectionFailure || E == EError::Timeout || E == EError::InvalidResponse || E == EError::ServiceUnavailable;
}
bool ValidTokens(const FAuthTokens& T, double Utc)
{
    return IsAsciiIdentity(T.PlayerId, 256) && IsAsciiIdentity(T.SessionId, 256) &&
        IsAsciiIdentity(T.AccessToken, 8192) && IsAsciiIdentity(T.RefreshToken, 8192) &&
        std::isfinite(T.AccessExpiresAt) && std::isfinite(T.RefreshExpiresAt) && T.AccessExpiresAt > Utc && T.RefreshExpiresAt >= T.AccessExpiresAt;
}
}

EError ValidateConfiguration(const FConfiguration& C, bool bShipping)
{
    const auto Positive = [](double V) { return std::isfinite(V) && V > 0; };
    if (!C.bVerifyCertificates || !IsOriginValid(C, bShipping) || !IsAsciiIdentity(C.GameId, 128) ||
        !IsAsciiIdentity(C.ClientVersion, 128) || C.ContractVersion != "1.0.0" ||
        !Positive(C.Deadline) || C.Deadline > 300 || !Positive(C.AttemptTimeout) || C.AttemptTimeout > C.Deadline ||
        !Positive(C.RevocationDeadline) || C.RevocationDeadline > 30 || !Positive(C.RetryBaseDelay) ||
        !Positive(C.MaxRetryDelay) || C.MaxRetryDelay > C.Deadline || C.RetryBaseDelay > C.MaxRetryDelay ||
        C.MaxConcurrent < 1 || C.MaxConcurrent > 32 || C.MaxQueued < 1 || C.MaxQueued > 1024 ||
        C.MaxRefreshWaiters < 1 || C.MaxRefreshWaiters > 1024 || C.MaxResponseBytes < 256 || C.MaxResponseBytes > 4194304 ||
        C.MaxRequestBytes < 256 || C.MaxRequestBytes > 65536 || C.MaxReadRetries < 0 || C.MaxReadRetries > 5)
        return EError::InvalidConfiguration;
    return EError::None;
}

bool IsDisplayNameValid(const std::string& S)
{
    if (S.empty() || S.size() > 96) return false;
    size_t Count = 0;
    for (size_t I = 0; I < S.size();)
    {
        auto C = static_cast<unsigned char>(S[I++]); std::uint32_t Value = C; int Remaining = 0;
        if (C < 0x80) { if (C < 32 || C == 127) return false; }
        else if (C >= 0xc2 && C <= 0xdf) { Value = C & 31; Remaining = 1; }
        else if (C >= 0xe0 && C <= 0xef) { Value = C & 15; Remaining = 2; }
        else if (C >= 0xf0 && C <= 0xf4) { Value = C & 7; Remaining = 3; }
        else return false;
        const int Extra = Remaining;
        while (Remaining-- > 0)
        {
            if (I >= S.size()) return false; const auto D = static_cast<unsigned char>(S[I++]);
            if ((D & 0xc0) != 0x80) return false; Value = (Value << 6) | (D & 63);
        }
        if ((Extra == 2 && Value < 0x800) || (Extra == 3 && Value < 0x10000) || Value > 0x10ffff || (Value >= 0xd800 && Value <= 0xdfff)) return false;
        if (++Count > 24) return false;
    }
    return true;
}

FSession::FSession(ITransport& InTransport, std::function<std::string()> NewIdentity)
    : Transport(InTransport), NewId(std::move(NewIdentity)) {}
FSession::~FSession() { Shutdown(); }
bool FSession::IsBound(EOperation Op) { return Op != EOperation::Probe && Op != EOperation::Logout; }
bool FSession::IsAlive(const FRequest& R) { return !R.Options.IsAlive || R.Options.IsAlive(); }

EError FSession::Configure(const FConfiguration& C)
{
    if (bStopped || !Requests.empty() || Flight || Auth.State != EAuthState::SignedOut) return EError::AuthenticationBusy;
    const auto Error = ValidateConfiguration(C, C.bShipping); if (Error != EError::None) return Error;
    if (!Transport.SupportsSecureRequests()) return EError::UnsupportedTransport;
    Config = C; Stats.bConfigured = true; Stats.Service = EServiceState::Unknown; return EError::None;
}

std::string FSession::Submit(EOperation Op, FInput Input, FOptions Options, FCompletion Completion, double Now, double UtcNow)
{
    Clock = Now; UtcClock = UtcNow;
    // Logout 的本地清理不依赖配置、安全能力、容量或服务器可达性。
    std::string Revoke;
    if (Op == EOperation::Logout) { Revoke = Tokens.RefreshToken; ClearAuthentication(); }
    auto R = std::make_shared<FRequest>(); R->Id = NewId(); R->Operation = Op;
    R->Input = std::move(Input); R->Options = std::move(Options); R->Complete = std::move(Completion);
    R->Generation = Auth.Generation; R->ContextId = Auth.ContextId;
    R->Started = Now; R->Deadline = Now + (Op == EOperation::Logout ? Config.RevocationDeadline : (R->Options.Deadline > 0 ? R->Options.Deadline : Config.Deadline));
    R->RevokeToken = std::move(Revoke);
    Requests.emplace(R->Id, R);
    EError Error = EError::None;
    if (bStopped) Error = EError::Cancelled;
    else if (!Stats.bConfigured) Error = EError::InvalidConfiguration;
    else if (!R->Options.bValid || !std::isfinite(R->Options.Deadline) || R->Options.Deadline < 0 || R->Options.Deadline > Config.Deadline || !R->Complete) Error = EError::InvalidArgument;
    else if (!IsAlive(*R)) Error = EError::Cancelled;
    else if (Diagnostics().Queued > Config.MaxQueued) Error = EError::QueueFull;
    else if (Op == EOperation::Login)
    {
        if (Auth.State != EAuthState::SignedOut) Error = EError::AuthenticationBusy;
        else if (R->Input.AccountName.empty() || R->Input.AccountName.size() > 256 || R->Input.Credential.empty() || R->Input.Credential.size() > 4096 || R->Input.DeviceId.size() > 256) Error = EError::InvalidArgument;
        else { ++Auth.Generation; Auth.ContextId = NewId(); Auth.State = EAuthState::SigningIn; R->Generation = Auth.Generation; R->ContextId = Auth.ContextId; }
    }
    else if (IsBound(Op) && Auth.State != EAuthState::SignedIn && Auth.State != EAuthState::Refreshing) Error = EError::Unauthenticated;
    if (Error == EError::None && Op == EOperation::UpdateProfile &&
        (!IsDisplayNameValid(R->Input.DisplayName) || R->Input.ExpectedRevision < 0 || !IsAsciiIdentity(R->Input.IdempotencyKey, 128))) Error = EError::InvalidArgument;
    if (Error != EError::None) Finish(R->Id, Error);
    else if (Op == EOperation::Logout && R->RevokeToken.empty()) Finish(R->Id, EError::None);
    else if (Op == EOperation::Refresh) JoinRefresh(R);
    return R->Id;
}

void FSession::Finish(const std::string& Id, EError Error, const FReply* Reply)
{
    auto It = Requests.find(Id); if (It == Requests.end()) return;
    auto R = It->second; Requests.erase(It); // 先移除再取消；同步晚回调只能进入邮箱，不能第二次完成。
    if (!R->AttemptId.empty()) Transport.Cancel(R->AttemptId);
    FOutcome O; O.Error = Error; O.RequestId = R->Id; O.Elapsed = std::max(0.0, Clock - R->Started);
    if (R->Operation == EOperation::Login && Error != EError::None && R->Generation == Auth.Generation && Auth.State == EAuthState::SigningIn)
    { ++Auth.Generation; Auth.ContextId.clear(); Auth.State = EAuthState::SignedOut; }
    if (R->Operation == EOperation::Logout)
        O.Logout = R->RevokeToken.empty() ? ELogout::LocalSignedOut : (Error == EError::None ? ELogout::ServerRevoked : ELogout::RevocationUnconfirmed);
    O.Authentication = Auth;
    if (Reply && Error == EError::None) { O.Profile = Reply->Profile; O.bReady = Reply->bReady; O.ContractVersion = Reply->ContractVersion; }
    if (R->Operation == EOperation::Probe)
        Stats.Service = Error == EError::None ? EServiceState::Ready : (Error == EError::IncompatibleProtocol ? EServiceState::Incompatible : EServiceState::Unavailable);
    ++Stats.Completed; Stats.LastError = Error;
    Deliveries.push_back({std::move(R->Complete), std::move(O), std::move(R->Options), R->Generation, IsBound(R->Operation)});
}

FReceive FSession::Receiver(std::string Id, std::string AttemptId, bool bRefresh)
{
    std::weak_ptr<FMailbox> Weak = Mailbox;
    return [Weak, Id = std::move(Id), AttemptId = std::move(AttemptId), bRefresh](FReply Reply)
    {
        if (auto Box = Weak.lock())
        {
            std::lock_guard<std::mutex> Lock(Box->Mutex);
            if (!Box->bClosed) Box->Events.push_back({Id, AttemptId, std::move(Reply), bRefresh});
        }
    };
}

int FSession::ActiveCount() const
{
    int Count = Flight && Flight->bStarted ? 1 : 0;
    for (const auto& Pair : Requests) if (!Pair.second->AttemptId.empty()) ++Count;
    return Count;
}

void FSession::BeginAttempt(const std::shared_ptr<FRequest>& R)
{
    if ((R->Operation == EOperation::ReadProfile || R->Operation == EOperation::UpdateProfile) &&
        (Auth.State == EAuthState::Refreshing || Tokens.AccessExpiresAt <= UtcClock))
    { JoinRefresh(R); return; }
    R->AttemptId = NewId(); R->AttemptDeadline = std::min(R->Deadline, Clock + Config.AttemptTimeout);
    R->SentTokenVersion = Auth.TokenVersion;
    FWireRequest W; W.Operation = R->Operation; W.RequestId = R->Id; W.AttemptId = R->AttemptId;
    W.OwnerScopeId = R->Options.OwnerScopeId; W.AuthContextId = R->ContextId; W.AuthGeneration = R->Generation;
    W.TokenVersion = R->SentTokenVersion; W.Configuration = Config; W.Input = R->Input; W.Timeout = R->AttemptDeadline - Clock;
    if (R->Operation == EOperation::ReadProfile || R->Operation == EOperation::UpdateProfile) W.AccessToken = Tokens.AccessToken;
    if (R->Operation == EOperation::Logout) W.RefreshToken = R->RevokeToken;
    // 登录不重试，交给传输后删除状态机中的密码副本。
    if (R->Operation == EOperation::Login) R->Input.Credential.clear();
    if (!Transport.Send(std::move(W), Receiver(R->Id, R->AttemptId, false))) Finish(R->Id, EError::ConnectionFailure);
}

void FSession::JoinRefresh(const std::shared_ptr<FRequest>& R)
{
    if (R->bWaitingRefresh) return;
    if (R->bAuthReplay && R->Operation != EOperation::Refresh) { Finish(R->Id, EError::Unauthenticated); return; }
    if (Tokens.RefreshToken.empty() || Tokens.RefreshExpiresAt <= UtcClock || Auth.State == EAuthState::ReauthenticationRequired)
    { Auth.State = EAuthState::ReauthenticationRequired; Finish(R->Id, EError::Unauthenticated); return; }
    if (Diagnostics().RefreshWaiters >= Config.MaxRefreshWaiters) { Finish(R->Id, EError::QueueFull); return; }
    R->bWaitingRefresh = true; R->bAuthReplay = true;
    if (!Flight)
    {
        FFlight F; F.AttemptId = NewId(); F.ContextId = Auth.ContextId; F.Generation = Auth.Generation;
        F.Deadline = Clock + std::min(Config.AttemptTimeout, Config.Deadline); Flight = std::move(F);
        Auth.State = EAuthState::Refreshing;
    }
}

void FSession::StartRefresh()
{
    if (!Flight || Flight->bStarted || ActiveCount() >= Config.MaxConcurrent) return;
    FWireRequest W; W.Operation = EOperation::Refresh; W.AttemptId = Flight->AttemptId; W.RequestId = NewId();
    W.AuthGeneration = Flight->Generation; W.AuthContextId = Flight->ContextId; W.TokenVersion = Auth.TokenVersion;
    W.Configuration = Config; W.RefreshToken = Tokens.RefreshToken; W.Timeout = Flight->Deadline - Clock;
    Flight->bStarted = true; ++Stats.RefreshAttempts;
    if (!Transport.Send(std::move(W), Receiver({}, Flight->AttemptId, true)))
    { FReply R; R.Error = EError::ConnectionFailure; R.bMayHaveReachedServer = false; CompleteRefresh(std::move(R)); }
}

void FSession::CompleteRefresh(FReply Reply)
{
    if (!Flight) return;
    const auto F = *Flight; Flight.reset(); Transport.Cancel(F.AttemptId);
    if (F.Generation != Auth.Generation || F.ContextId != Auth.ContextId) return;
    if (Reply.Error == EError::None && (!ValidTokens(Reply.Auth, UtcClock) || Reply.Auth.PlayerId != Tokens.PlayerId || Reply.Auth.SessionId != Tokens.SessionId)) Reply.Error = EError::InvalidResponse;
    if (Reply.Error != EError::None)
    {
        if (Reply.bMayHaveReachedServer && IsUncertain(Reply.Error)) Reply.Error = EError::OutcomeUnknown;
        // 即使明确失败，也不允许本次共享刷新结束后由等待者形成循环。
        Auth.State = EAuthState::ReauthenticationRequired;
    }
    else
    {
        Tokens = std::move(Reply.Auth); ++Auth.TokenVersion; Auth.State = EAuthState::SignedIn;
        Auth.AccessExpiresAt = Tokens.AccessExpiresAt; Auth.RefreshExpiresAt = Tokens.RefreshExpiresAt;
    }
    std::vector<std::string> Waiters;
    for (auto& Pair : Requests) if (Pair.second->bWaitingRefresh) Waiters.push_back(Pair.first);
    for (const auto& Id : Waiters)
    {
        auto R = Requests.at(Id); R->bWaitingRefresh = false;
        if (Reply.Error != EError::None || R->Operation == EOperation::Refresh) Finish(Id, Reply.Error);
        else R->ReadyAt = Clock;
    }
}

void FSession::HandleReply(FEvent Event)
{
    if (Event.bRefresh)
    { if (Flight && Flight->AttemptId == Event.AttemptId) CompleteRefresh(std::move(Event.Reply)); return; }
    auto It = Requests.find(Event.RequestId); if (It == Requests.end()) return;
    auto R = It->second; if (R->AttemptId != Event.AttemptId) return;
    R->AttemptId.clear();
    if (!IsAlive(*R) || (IsBound(R->Operation) && R->Generation != Auth.Generation)) { Finish(R->Id, EError::Cancelled); return; }
    auto& Reply = Event.Reply;
    if (Clock >= R->Deadline) { Finish(R->Id, R->Operation == EOperation::UpdateProfile || R->Operation == EOperation::Login ? EError::OutcomeUnknown : EError::Timeout); return; }
    if (Reply.Error == EError::Unauthenticated && (R->Operation == EOperation::ReadProfile || R->Operation == EOperation::UpdateProfile))
    {
        if (R->bAuthReplay) { Auth.State = EAuthState::ReauthenticationRequired; Finish(R->Id, EError::Unauthenticated); }
        else if (R->SentTokenVersion < Auth.TokenVersion && Auth.State == EAuthState::SignedIn)
        { R->bAuthReplay = true; R->ReadyAt = Clock; }
        else JoinRefresh(R);
        return;
    }
    if (Reply.Error != EError::None)
    {
        if ((R->Operation == EOperation::Probe || R->Operation == EOperation::ReadProfile) && IsTransient(Reply.Error) && R->Retries < Config.MaxReadRetries)
        {
            // 每次尝试身份产生可重复但不同的有界抖动；不是安全随机数用途。
            const double Jitter = .8 + static_cast<double>(std::hash<std::string>{}(Event.AttemptId) % 401) / 1000.0;
            const double Delay = std::max(Reply.RetryAfter, Config.RetryBaseDelay * std::pow(2.0, R->Retries) * Jitter);
            if (std::isfinite(Delay) && Delay <= Config.MaxRetryDelay && Clock + Delay < R->Deadline)
            { ++R->Retries; R->ReadyAt = Clock + Delay; return; }
        }
        if ((R->Operation == EOperation::UpdateProfile || R->Operation == EOperation::Login) && Reply.bMayHaveReachedServer && IsUncertain(Reply.Error)) Reply.Error = EError::OutcomeUnknown;
        Finish(R->Id, Reply.Error); return;
    }
    if (R->Operation == EOperation::Login)
    {
        if (!ValidTokens(Reply.Auth, UtcClock)) { Finish(R->Id, EError::OutcomeUnknown); return; }
        Tokens = std::move(Reply.Auth); Auth.PlayerId = Tokens.PlayerId; Auth.State = EAuthState::SignedIn;
        Auth.TokenVersion = 1; Auth.AccessExpiresAt = Tokens.AccessExpiresAt; Auth.RefreshExpiresAt = Tokens.RefreshExpiresAt;
    }
    else if (R->Operation == EOperation::Probe)
    {
        if (Reply.ContractVersion != Config.ContractVersion) { Finish(R->Id, EError::IncompatibleProtocol); return; }
        if (!Reply.bReady) { Finish(R->Id, EError::ServiceUnavailable); return; }
    }
    else if (R->Operation == EOperation::ReadProfile || R->Operation == EOperation::UpdateProfile)
    {
        if (Reply.Profile.PlayerId != Auth.PlayerId || Reply.Profile.GameId != Config.GameId || Reply.Profile.Revision < 0 || Reply.Profile.DataVersion < 1)
        { Finish(R->Id, EError::InvalidResponse); return; }
        if (!Cache || Reply.Profile.Revision >= Cache->Revision) Cache = Reply.Profile;
    }
    Finish(R->Id, EError::None, &Reply);
}

bool FSession::Cancel(const std::string& Id)
{
    if (Requests.find(Id) == Requests.end()) return false;
    Finish(Id, EError::Cancelled); return true;
}

void FSession::ClearAuthentication()
{
    ++Auth.Generation; Auth.ContextId.clear(); Auth.PlayerId.clear(); Auth.State = EAuthState::SignedOut;
    Auth.TokenVersion = 0; Auth.AccessExpiresAt = Auth.RefreshExpiresAt = 0; Tokens = {}; Cache.reset();
    if (Flight) { auto Id = Flight->AttemptId; Flight.reset(); Transport.Cancel(Id); }
    std::vector<std::string> Bound;
    for (const auto& Pair : Requests) if (IsBound(Pair.second->Operation)) Bound.push_back(Pair.first);
    for (const auto& Id : Bound) Finish(Id, EError::Cancelled);
}

void FSession::Tick(double Now, double UtcNow)
{
    if (bTicking) return; bTicking = true; Clock = Now; UtcClock = UtcNow;
    // 只交付 Tick 开始时已有的终态；本 Tick 收到的回调和重入提交留到下一次。
    auto Ready = std::move(Deliveries); Deliveries.clear();
    for (auto& D : Ready)
    {
        if (D.Outcome.Error == EError::None && ((D.bAuthenticationBound && D.Generation != Auth.Generation) || (D.Options.IsAlive && !D.Options.IsAlive())))
        { D.Outcome.Error = EError::Cancelled; D.Outcome.Profile = {}; D.Outcome.Authentication = Auth; }
        if (D.Complete) D.Complete(std::move(D.Outcome));
    }
    std::vector<std::string> Snapshot; for (const auto& Pair : Requests) Snapshot.push_back(Pair.first);
    for (const auto& Id : Snapshot)
    {
        auto It = Requests.find(Id); if (It == Requests.end()) continue; auto R = It->second;
        if (!IsAlive(*R)) Finish(Id, EError::Cancelled);
        else if (Clock >= R->Deadline || (!R->AttemptId.empty() && Clock >= R->AttemptDeadline))
        {
            const bool bWriteDispatched = !R->AttemptId.empty() && (R->Operation == EOperation::Login || R->Operation == EOperation::UpdateProfile);
            Finish(Id, bWriteDispatched ? EError::OutcomeUnknown : EError::Timeout);
        }
    }
    if (Flight && Clock >= Flight->Deadline)
    { FReply R; R.Error = EError::Timeout; R.bMayHaveReachedServer = Flight->bStarted; CompleteRefresh(std::move(R)); }
    std::deque<FEvent> Events;
    { std::lock_guard<std::mutex> Lock(Mailbox->Mutex); Events.swap(Mailbox->Events); }
    for (auto& E : Events) HandleReply(std::move(E));
    if (!bStopped)
    {
        StartRefresh();
        Snapshot.clear(); for (const auto& Pair : Requests) Snapshot.push_back(Pair.first);
        for (const auto& Id : Snapshot)
        {
            auto It = Requests.find(Id); if (It == Requests.end()) continue; auto R = It->second;
            if (ActiveCount() >= Config.MaxConcurrent) break;
            if (R->AttemptId.empty() && !R->bWaitingRefresh && R->ReadyAt <= Clock && R->Operation != EOperation::Refresh) BeginAttempt(R);
        }
        StartRefresh();
    }
    bTicking = false;
}

void FSession::Shutdown()
{
    if (bStopped) return; bStopped = true; ClearAuthentication();
    std::vector<std::string> Ids; for (const auto& Pair : Requests) Ids.push_back(Pair.first);
    for (const auto& Id : Ids) Finish(Id, EError::Cancelled);
    std::lock_guard<std::mutex> Lock(Mailbox->Mutex); Mailbox->bClosed = true; Mailbox->Events.clear();
}
bool FSession::HasWork() const { return !Requests.empty() || Flight.has_value() || !Deliveries.empty(); }
FDiagnostics FSession::Diagnostics() const
{
    auto D = Stats; D.Active = ActiveCount();
    for (const auto& Pair : Requests)
    {
        if (Pair.second->bWaitingRefresh) ++D.RefreshWaiters;
        else if (Pair.second->AttemptId.empty()) ++D.Queued;
    }
    return D;
}
}
