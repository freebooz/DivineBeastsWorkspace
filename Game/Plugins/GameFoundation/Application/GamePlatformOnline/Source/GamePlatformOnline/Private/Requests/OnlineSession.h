#pragma once

// 生产纯逻辑；不访问 UObject、不执行网络、不读取环境。所有状态方法由门面在 GT 串行调用。
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace GamePlatformOnline::Internal
{
enum class EError : std::uint8_t
{
    None, InvalidArgument, InvalidConfiguration, UnsupportedTransport, ConnectionFailure,
    TlsFailure, Timeout, Cancelled, InvalidResponse, IncompatibleProtocol, Unauthenticated,
    Forbidden, Conflict, NotFound, RateLimited, ServiceUnavailable, OutcomeUnknown, QueueFull, AuthenticationBusy
};
enum class EOperation { Probe, Login, ReadProfile, UpdateProfile, Refresh, Logout };
enum class EAuthState { SignedOut, SigningIn, SignedIn, Refreshing, ReauthenticationRequired };
enum class ELogout { NotExecuted, LocalSignedOut, ServerRevoked, RevocationUnconfirmed };
enum class EServiceState { Unknown, Ready, Unavailable, Incompatible };

struct FConfiguration
{
    std::string Origin, GameId, ClientVersion, ContractVersion = "1.0.0";
    bool bVerifyCertificates = true, bAllowLoopbackHttp = false, bShipping = false;
    double Deadline = 30, AttemptTimeout = 10, RevocationDeadline = 5;
    int MaxConcurrent = 4, MaxQueued = 32, MaxRefreshWaiters = 32;
    int MaxResponseBytes = 262144, MaxRequestBytes = 16384, MaxReadRetries = 2;
    double RetryBaseDelay = .25, MaxRetryDelay = 5;
};
EError ValidateConfiguration(const FConfiguration& C, bool bShipping);
/** 严格 UTF-8 标量计数；无效编码、控制字符、超限直接拒绝，不以字节数冒充 Unicode 字符数。 */
bool IsDisplayNameValid(const std::string& Text);

struct FAuthTokens
{
    std::string PlayerId, SessionId, AccessToken, RefreshToken;
    double AccessExpiresAt = 0, RefreshExpiresAt = 0; // UTC Unix 秒，只在私有内存保存。
};
struct FAuthentication
{
    EAuthState State = EAuthState::SignedOut;
    std::string ContextId, PlayerId;
    std::uint64_t Generation = 0, TokenVersion = 0;
    double AccessExpiresAt = 0, RefreshExpiresAt = 0;
};
struct FProfile
{
    std::string PlayerId, GameId, DisplayName, DefaultWorldId;
    std::int64_t Revision = -1;
    std::int32_t DataVersion = 0;
    bool bTutorialCompleted = false;
    std::vector<std::string> OwnedCharacterIds;
};
struct FInput
{
    std::string AccountName, Credential, DeviceId, DisplayName, IdempotencyKey;
    std::int64_t ExpectedRevision = -1;
};
struct FOptions
{
    double Deadline = 0;
    bool bValid = true;
    std::string OwnerScopeId;
    std::function<bool()> IsAlive; // 仅 GT 调用；由门面弱捕获调用者与世界。
};
struct FReply
{
    EError Error = EError::InvalidResponse;
    bool bMayHaveReachedServer = true, bReady = false;
    double RetryAfter = 0;
    std::string ContractVersion;
    FAuthTokens Auth;
    FProfile Profile;
};
struct FOutcome
{
    EError Error = EError::InvalidResponse;
    std::string RequestId;
    double Elapsed = 0;
    bool bReady = false;
    std::string ContractVersion;
    FAuthentication Authentication;
    FProfile Profile;
    ELogout Logout = ELogout::NotExecuted;
};
/** 私有传输输入；令牌和请求输入不能复制到公共结果、日志或持久化。 */
struct FWireRequest
{
    EOperation Operation = EOperation::Probe;
    std::string RequestId, AttemptId, OwnerScopeId, AuthContextId;
    std::uint64_t AuthGeneration = 0, TokenVersion = 0;
    FConfiguration Configuration;
    FInput Input;
    std::string AccessToken, RefreshToken;
    double Timeout = 0;
};
using FReceive = std::function<void(FReply)>;
class ITransport
{
public:
    virtual ~ITransport() = default;
    /** 必须探测真实安全能力，不允许用 SetOption 的字符串回显判断功能。 */
    virtual bool SupportsSecureRequests() const = 0;
    /** false 表示未发起；回调可来自任意线程甚至同步，状态机统一排入邮箱。 */
    virtual bool Send(FWireRequest Request, FReceive Receive) = 0;
    virtual void Cancel(const std::string& AttemptId) = 0;
};
/** 缺能力环境明确失败；无网络、无成功结果。主要供拒绝路径与无 HTTP 环境使用。 */
class FBlockedTransport final : public ITransport
{
public:
    bool SupportsSecureRequests() const override { return false; }
    bool Send(FWireRequest, FReceive Receive) override
    { FReply R; R.Error = EError::UnsupportedTransport; R.bMayHaveReachedServer = false; Receive(std::move(R)); return false; }
    void Cancel(const std::string&) override {} // 没有发起请求，因此没有底层资源。
};
struct FDiagnostics
{
    bool bConfigured = false;
    EServiceState Service = EServiceState::Unknown;
    int Active = 0, Queued = 0, RefreshWaiters = 0;
    std::uint64_t RefreshAttempts = 0, Completed = 0;
    EError LastError = EError::None;
};

/** 生产请求与认证执行器；身份工厂来自实例门面 FGuid，不共享进程当前账号。 */
class FSession
{
public:
    using FCompletion = std::function<void(FOutcome)>;
    FSession(ITransport& Transport, std::function<std::string()> NewIdentity);
    ~FSession();
    EError Configure(const FConfiguration& Configuration);
    std::string Submit(EOperation Operation, FInput Input, FOptions Options, FCompletion Completion, double Now, double UtcNow);
    bool Cancel(const std::string& RequestId);
    void Tick(double Now, double UtcNow);
    void Shutdown();
    bool HasWork() const;
    FAuthentication Authentication() const { return Auth; }
    std::optional<FProfile> CachedProfile() const { return Cache; }
    FDiagnostics Diagnostics() const;

private:
    struct FRequest
    {
        std::string Id, AttemptId, ContextId, RevokeToken;
        EOperation Operation = EOperation::Probe;
        FInput Input;
        FOptions Options;
        FCompletion Complete;
        std::uint64_t Generation = 0, SentTokenVersion = 0;
        double Started = 0, Deadline = 0, AttemptDeadline = 0, ReadyAt = 0;
        int Retries = 0;
        bool bWaitingRefresh = false, bAuthReplay = false;
    };
    struct FFlight
    {
        std::string AttemptId, ContextId;
        std::uint64_t Generation = 0;
        double Deadline = 0;
        bool bStarted = false;
    };
    struct FDelivery
    {
        FCompletion Complete;
        FOutcome Outcome;
        FOptions Options;
        std::uint64_t Generation = 0;
        bool bAuthenticationBound = false;
    };
    struct FEvent { std::string RequestId, AttemptId; FReply Reply; bool bRefresh = false; };
    struct FMailbox { std::mutex Mutex; std::deque<FEvent> Events; bool bClosed = false; };
    ITransport& Transport;
    std::function<std::string()> NewId;
    std::shared_ptr<FMailbox> Mailbox = std::make_shared<FMailbox>();
    FConfiguration Config;
    FAuthentication Auth;
    FAuthTokens Tokens;
    std::optional<FProfile> Cache;
    std::map<std::string, std::shared_ptr<FRequest>> Requests;
    std::optional<FFlight> Flight;
    std::deque<FDelivery> Deliveries;
    FDiagnostics Stats;
    double Clock = 0, UtcClock = 0;
    bool bStopped = false, bTicking = false;
    static bool IsBound(EOperation Op);
    static bool IsAlive(const FRequest& R);
    int ActiveCount() const;
    void Finish(const std::string& Id, EError Error, const FReply* Reply = nullptr);
    void BeginAttempt(const std::shared_ptr<FRequest>& R);
    void HandleReply(FEvent Event);
    void JoinRefresh(const std::shared_ptr<FRequest>& R);
    void StartRefresh();
    void CompleteRefresh(FReply Reply);
    void ClearAuthentication();
    FReceive Receiver(std::string Id, std::string AttemptId, bool bRefresh);
};
}
