#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Settings/GamePlatformOnlineConfiguration.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformOnlineDiagnostics.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "GamePlatformOnlineClientSubsystem.generated.h"

class IHttpRequest;

/** EGamePlatformAuthState（平台客户端认证状态）。 */
UENUM(BlueprintType)
enum class EGamePlatformAuthState : uint8
{
    LoggedOut,
    LoggingIn,
    Authenticated,
    Refreshing,
    LoggingOut,
    Failed
};

/** EGamePlatformAuthError（平台客户端认证/受保护请求错误）。 */
UENUM(BlueprintType)
enum class EGamePlatformAuthError : uint8
{
    None,
    ProviderUnavailable,
    InvalidCredentials,
    AccountLocked,
    Maintenance,
    NetworkUnavailable,
    AuthExpired,
    ContractIncompatible,
    Cancelled,
    TimedOut,
    QueueFull,
    AuthenticationBusy,
    InvalidRequest,
    InvalidResponse,
    Forbidden,
    Conflict,
    NotFound,
    RateLimited,
    ServiceUnavailable,
    OutcomeUnknown,
    Unknown
};

/**
 * FGamePlatformAuthSnapshot（平台认证公开快照）。
 * 只包含非秘密事实；AccessToken/RefreshToken 永远不进入 UObject、UI、日志或遥测。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EGamePlatformAuthState State = EGamePlatformAuthState::LoggedOut;
    UPROPERTY(BlueprintReadOnly) EGamePlatformAuthError Error = EGamePlatformAuthError::None;
    UPROPERTY(BlueprintReadOnly) FString AccountId;
    UPROPERTY(BlueprintReadOnly) FString SessionId;
    UPROPERTY(BlueprintReadOnly) FDateTime AccessExpiresAt;
    UPROPERTY(BlueprintReadOnly) FDateTime RefreshExpiresAt;
    /** 登录/退出/切换Provider才推进；成功刷新不会改变认证上下文代次。 */
    UPROPERTY(BlueprintReadOnly) FGuid AuthGeneration;
};

/** Provider完成的非秘密认证结果。 */
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthProviderResult
{
    bool bSuccess = false;
    FString AccountId;
    FString SessionId;
    FDateTime AccessExpiresAt;
    FDateTime RefreshExpiresAt;
    EGamePlatformAuthError Error = EGamePlatformAuthError::Unknown;
};

using FGamePlatformAuthCompletion =
    TFunction<void(FGamePlatformAuthProviderResult)>;

/** Provider退出结果；本地状态由子系统先清理，本值只描述远端撤销是否得到确认。 */
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthProviderLogoutResult
{
    bool bServerRevoked = false;
    EGamePlatformAuthError Error = EGamePlatformAuthError::None;
};

using FGamePlatformAuthLogoutCompletion =
    TFunction<void(FGamePlatformAuthProviderLogoutResult)>;

/**
 * FGamePlatformAuthenticatedRequest（同源受保护HTTP请求）。
 * 只允许相对路径；Authorization由Provider私有注入，项目层永远拿不到Token。
 */
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthenticatedRequest
{
    FString Verb;
    FString RelativePath;
    FString Body;
    /** 可选业务幂等键；写请求声明可重放时必须提供，平台统一写入 Idempotency-Key 请求头。 */
    FString IdempotencyKey;
    /** GET/HEAD天然可安全重放；写请求只有同时提供业务幂等键时才允许在刷新后重放一次。 */
    bool bIdempotent = false;
};

/** 受保护请求结果；正文受统一字节预算限制。 */
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthenticatedResponse
{
    EGamePlatformAuthError Error = EGamePlatformAuthError::Unknown;
    int32 HttpStatusCode = 0;
    FString Body;
    bool bMayHaveReachedServer = false;

    bool IsSuccess() const
    {
        return Error == EGamePlatformAuthError::None &&
            HttpStatusCode >= 200 && HttpStatusCode < 300;
    }
};

using FGamePlatformAuthenticatedCompletion =
    TFunction<void(FGamePlatformAuthenticatedResponse)>;

/**
 * IGamePlatformOnlineAuthProvider（平台在线传输提供者）。
 *
 * Provider拥有原始Token与HTTP资源；上层子系统只观察无秘密快照。
 * 所有回调允许从任意线程完成，子系统必须统一切回游戏线程后提交状态。
 */
class GAMEPLATFORMONLINECLIENT_API IGamePlatformOnlineAuthProvider
{
public:
    virtual ~IGamePlatformOnlineAuthProvider() = default;

    virtual FGamePlatformResult Configure(
        const FGamePlatformOnlineConfiguration& Configuration) = 0;
    virtual void TryAutoLogin(FGamePlatformAuthCompletion Completion) = 0;
    virtual void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password,
        FGamePlatformAuthCompletion Completion) = 0;
    virtual void Refresh(FGamePlatformAuthCompletion Completion) = 0;
    virtual void Logout(FGamePlatformAuthLogoutCompletion Completion) = 0;

    /** 仅用于公开探测等无需认证的同源请求；仍必须经过统一URL、重定向、正文预算与超时策略。 */
    virtual void SendUnauthenticatedRequest(
        const FGuid& RequestId,
        const FGamePlatformAuthenticatedRequest& Request,
        FGamePlatformAuthenticatedCompletion Completion) = 0;

    virtual void SendAuthenticatedRequest(
        const FGuid& RequestId,
        const FGamePlatformAuthenticatedRequest& Request,
        FGamePlatformAuthenticatedCompletion Completion) = 0;
    virtual void CancelRequest(const FGuid& RequestId) = 0;
    /** 仅把当前AccessToken写入已经由可信平台模块创建的HTTP请求；不返回Token字符串。 */
    virtual bool ApplyAuthorization(IHttpRequest& Request) const = 0;
    virtual void CancelAll() = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAuthSnapshotChangedNative,
    const FGamePlatformAuthSnapshot&);

/**
 * UGamePlatformOnlineClientSubsystem（游戏平台在线客户端子系统）。
 *
 * 职责：
 * - 每个GameInstance独立维护一个认证上下文；
 * - 管理登录、退出、令牌轮换和公开无秘密快照；
 * - 对受保护请求实施并发/排队/截止时间预算；
 * - 401只触发一次共享刷新（single flight），403绝不刷新；
 * - 写请求只有调用方声明幂等时才允许在认证刷新后重放一次；
 * - 只在存在排队/活动请求时启用低频Ticker，不执行逐帧网络轮询。
 */
UCLASS()
class GAMEPLATFORMONLINECLIENT_API UGamePlatformOnlineClientSubsystem final
    : public UGameInstanceSubsystem
    , public IGamePlatformOnlineService
{
    GENERATED_BODY()

public:
    UGamePlatformOnlineClientSubsystem();
    virtual ~UGamePlatformOnlineClientSubsystem() override;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 组合根显式配置；不读取项目环境变量，不内置具体游戏地址。 */
    virtual FGamePlatformResult Configure(
        const FGamePlatformOnlineConfiguration& InConfiguration) override;

    /** IGamePlatformOnlineService（统一在线服务门面）实现；主工程与平台插件统一通过同一GameInstance实例访问。 */
    virtual FGamePlatformOnlineRequestHandle ProbeService(
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineProbeCompletion Completion) override;
    virtual FGamePlatformOnlineRequestHandle Login(
        FGamePlatformOnlineLoginRequest Request,
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineAuthenticationCompletion Completion) override;
    virtual FGamePlatformOnlineRequestHandle GetCurrentPlayerProfile(
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineProfileCompletion Completion) override;
    virtual FGamePlatformOnlineRequestHandle UpdateCurrentPlayerProfile(
        const FGamePlatformOnlineProfileUpdateRequest& Request,
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineProfileCompletion Completion) override;
    virtual FGamePlatformOnlineRequestHandle RefreshAuthentication(
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineAuthenticationCompletion Completion) override;
    virtual FGamePlatformOnlineRequestHandle Logout(
        FGamePlatformOnlineLogoutCompletion Completion) override;

    /**
     * 替换Provider主要用于自动化测试或宿主适配。
     * 切换会取消全部旧请求、清空认证并推进代次，防止迟到回调污染新上下文。
     */
    void SetProvider(TSharedPtr<IGamePlatformOnlineAuthProvider> InProvider);

    void TryAutoLogin();
    void LoginWithCredentials(const FString& LoginName, const FString& Password);
    void Refresh();
    void Logout();

    /**
     * 发起同一已配置Gateway上的受保护请求。
     * 返回句柄只用于本GameInstance取消；失败结果仍异步完成一次。
     */
    FGamePlatformOnlineRequestHandle SendAuthenticatedRequest(
        FGamePlatformAuthenticatedRequest Request,
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformAuthenticatedCompletion Completion);

    virtual bool Cancel(
        const FGamePlatformOnlineRequestHandle& Request) override;

    /** 供其他平台基础插件（如Telemetry）在不读取Token字符串的情况下给HTTP请求附加认证。 */
    bool ApplyAuthorization(IHttpRequest& Request) const;

    FGamePlatformAuthSnapshot GetSnapshot() const { return Snapshot; }
    virtual FGamePlatformOnlineAuthSnapshot GetAuthentication() const override;
    virtual TOptional<FGamePlatformOnlineProfile> GetCachedProfile() const override;
    virtual FGamePlatformOnlineDiagnostics GetDiagnostics() const override;

    FGamePlatformAuthSnapshotChangedNative& OnAuthStateChanged()
    {
        return AuthStateChanged;
    }

private:
    struct FRuntime;

    void ResetAuthentication(
        EGamePlatformAuthState NewState,
        EGamePlatformAuthError Error,
        bool bAdvanceGeneration);
    void CompleteAuthentication(
        const FGuid& ExpectedGeneration,
        FGamePlatformAuthProviderResult Result,
        bool bIsRefresh);
    void BeginRefreshSingleFlight();
    void QueueForRefresh(const FGuid& RequestId);
    void PumpRequests();
    void CompleteRequest(
        const FGuid& RequestId,
        FGamePlatformAuthenticatedResponse Response);
    bool TickRequests(float DeltaSeconds);
    void EnsureTicker();
    void StopTicker();
    bool IsRequestAlive(const FGamePlatformOnlineRequestOptions& Options) const;
    void BroadcastSnapshot();
    FGamePlatformOnlineRequestHandle SendRequestInternal(
        FGamePlatformAuthenticatedRequest Request,
        const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformAuthenticatedCompletion Completion,
        bool bRequiresAuthentication);

    TUniquePtr<FRuntime> Runtime;
    FGamePlatformOnlineConfiguration Configuration;
    FGamePlatformAuthSnapshot Snapshot;
    TOptional<FGamePlatformOnlineProfile> CachedProfile;
    FGamePlatformAuthSnapshotChangedNative AuthStateChanged;
    FGuid InstanceScopeId;
    bool bConfigured = false;
};

