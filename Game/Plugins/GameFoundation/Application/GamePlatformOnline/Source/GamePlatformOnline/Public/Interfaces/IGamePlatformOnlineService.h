#pragma once

#include "Settings/GamePlatformOnlineConfiguration.h"
#include "Types/GamePlatformOnlineDiagnostics.h"

class UGameInstance;

/** 探测完成函数；成功与失败均在后续游戏线程完成队列执行。 */
using FGamePlatformOnlineProbeCompletion = TFunction<void(const FGamePlatformOnlineProbeResult&)>;
/** 登录／刷新完成函数；不能取得密码、访问令牌或刷新令牌。 */
using FGamePlatformOnlineAuthenticationCompletion = TFunction<void(const FGamePlatformOnlineAuthenticationResult&)>;
/** 读取／更新完成函数；载荷只在回调期间借用，跨回调保存需复制。 */
using FGamePlatformOnlineProfileCompletion = TFunction<void(const FGamePlatformOnlineProfileResult&)>;
/** 退出完成函数；远端撤销失败不得恢复旧认证。 */
using FGamePlatformOnlineLogoutCompletion = TFunction<void(const FGamePlatformOnlineLogoutResult&)>;

/**
 * 显式游戏实例的类型化在线门面；实现为 Private 中的 UGameInstanceSubsystem。
 * 全部方法只能在游戏线程调用；不持有全局当前玩家，不自动登录，不支持实例内分屏多账号。
 * 所有异步入口必须提供完成函数；受理后即便参数无效、命中缓存或发起失败，也在后续 GT 队列完成一次。
 * Owner/World 失效会令请求以 Cancelled 终结；回调仍执行，调用者必须弱捕获 UObject 并自行检查后再访问。
 * 服务引用不得跨游戏实例销毁持有；取消本地等待不表示服务器已回滚。
 */
class GAMEPLATFORMONLINE_API IGamePlatformOnlineService
{
public:
    virtual ~IGamePlatformOnlineService() = default;

    /** 从明确实例取得非拥有指针；不适用环境或子系统不存在时返回 nullptr，不使用 GWorld 或创建替代实例。 */
    static IGamePlatformOnlineService* Get(UGameInstance& GameInstance);

    /** 纯配置校验，不发网络、不存凭据；只校验配置值，不能代替 Configure 的实际传输能力检查。 */
    static FGamePlatformResult ValidateConfiguration(const FGamePlatformOnlineConfiguration& Configuration);

    /** 显式配置本实例；必须未认证且无未完成请求。安全传输能力不足时失败，禁止接受后静默降低保证。 */
    virtual FGamePlatformResult Configure(const FGamePlatformOnlineConfiguration& Configuration) = 0;

    /** 无认证探测依赖就绪与协议；不启动登录，不以成功发出请求作为就绪。 */
    virtual FGamePlatformOnlineRequestHandle ProbeService(const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineProbeCompletion Completion) = 0;

    /** 密码登录；输入按值移入临时请求，调用者宜 MoveTemp。登录或已认证期间再次登录明确失败，切账号先 Logout。 */
    virtual FGamePlatformOnlineRequestHandle Login(FGamePlatformOnlineLoginRequest Request,
        const FGamePlatformOnlineRequestOptions& Options, FGamePlatformOnlineAuthenticationCompletion Completion) = 0;

    /** 总是读取真实后端；需要当前认证，401 受一次认证重放限制，403 不刷新。 */
    virtual FGamePlatformOnlineRequestHandle GetCurrentPlayerProfile(const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineProfileCompletion Completion) = 0;

    /** 更新 DisplayName；ExpectedRevision 与 IdempotencyKey 必填，远端结果不确定不能冒充失败回滚或成功提交。 */
    virtual FGamePlatformOnlineRequestHandle UpdateCurrentPlayerProfile(const FGamePlatformOnlineProfileUpdateRequest& Request,
        const FGamePlatformOnlineRequestOptions& Options, FGamePlatformOnlineProfileCompletion Completion) = 0;

    /** 加入当前上下文的唯一刷新操作；取消一个句柄仅取消该等待者。轮换响应丢失不得自动重发旧令牌。 */
    virtual FGamePlatformOnlineRequestHandle RefreshAuthentication(const FGamePlatformOnlineRequestOptions& Options,
        FGamePlatformOnlineAuthenticationCompletion Completion) = 0;

    /**
     * 调用返回前先失效旧代次、清理认证和资料并取消旧请求，再以短期独立快照请求后端撤销。
     * 该操作固定为实例级，不依赖旧世界／页面；取消只停止等待撤销，绝不撤回本地退出。
     * 随后的显式 Login 建立新上下文，旧撤销完成不能修改它。
     */
    virtual FGamePlatformOnlineRequestHandle Logout(FGamePlatformOnlineLogoutCompletion Completion) = 0;

    /** 仅取消本实例仍活动的句柄；有效取消返回 true，已完成／外来句柄返回 false，终态仍异步送达一次。 */
    virtual bool Cancel(const FGamePlatformOnlineRequestHandle& Request) = 0;

    /** 取得无秘密认证值快照；不会发网络，也不以网络失联自动宣布凭据撤销。 */
    virtual FGamePlatformOnlineAuthSnapshot GetAuthentication() const = 0;

    /** 当前上下文没有已验证缓存时返回未设置；返回副本，缓存修订只能单调增加，Logout 清理。 */
    virtual TOptional<FGamePlatformOnlineProfile> GetCachedProfile() const = 0;

    /** 取得无凭据、无主体信息的实例计数快照；不包含原始 HTTP／JSON。 */
    virtual FGamePlatformOnlineDiagnostics GetDiagnostics() const = 0;
};
