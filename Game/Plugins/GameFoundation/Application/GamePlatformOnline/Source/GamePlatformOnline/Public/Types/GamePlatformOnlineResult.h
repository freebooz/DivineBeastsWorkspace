#pragma once

#include "Types/GamePlatformResult.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "Types/GamePlatformOnlineAuthentication.h"
#include "Types/GamePlatformOnlineProfile.h"

/** 中立在线错误分类；Result 仍复用 Core，不公开传输对象、JSON 或原始服务端错误正文。 */
enum class EGamePlatformOnlineError : uint8
{
    None,                    // 无额外错误；默认 Core 结果仍为未执行，不代表成功。
    InvalidArgument,         // 参数缺失、范围错误或所属实例不匹配。
    InvalidConfiguration,    // 地址、安全选项或预算配置不合法。
    UnsupportedTransport,    // 当前传输无法保证所要求的安全属性。
    ConnectionFailure,       // 可证明连接失败；无法区分 TLS 时不得猜测。
    TlsFailure,              // 仅在底层提供确定的证书／TLS 失败证据时使用。
    Timeout,                 // 总截止时间或单次尝试超时。
    Cancelled,               // 本地取消；不代表服务器事务已回滚。
    InvalidResponse,         // 内容类型、大小、必填字段或数值不合法。
    IncompatibleProtocol,    // 实际服务身份或契约版本不兼容。
    Unauthenticated,         // 缺少认证或明确凭据失效。
    Forbidden,               // 权限不足；不得自动刷新。
    Conflict,                // 修订或幂等冲突，禁止静默覆盖。
    NotFound,                // 当前主体所需资料不存在，不制造默认资料。
    RateLimited,             // 服务限流；受重试及截止时间预算约束。
    ServiceUnavailable,      // 服务尚未就绪或临时不可用，不等同退出。
    OutcomeUnknown,          // 写入或令牌轮换可能已发生，不能盲目重放。
    QueueFull,               // 逻辑队列或刷新等待队列达到上限。
    AuthenticationBusy       // 已登录或正在登录其他认证上下文，要求先显式退出。
};

/** 所有异步操作共有的脱敏终态；由 GT 完成队列至多一次送达，默认值不是成功。 */
struct FGamePlatformOnlineResult
{
    /** Core 中立结果；程序应先检查 Result.IsSuccess() 再读取成功载荷。 */
    FGamePlatformResult Result;
    /** 在线领域错误，不用原始 HTTP 状态或响应正文代替。 */
    EGamePlatformOnlineError Error = EGamePlatformOnlineError::None;
    /** 对应公开调用返回的逻辑句柄。 */
    FGamePlatformOnlineRequestHandle Request;
    /** 包含排队、刷新等待和重试的实际耗时，秒。 */
    double ElapsedSeconds = 0.0;
};

/** 服务探测终态；只有实际依赖就绪、服务身份和协议匹配才成功。 */
struct FGamePlatformOnlineProbeResult : FGamePlatformOnlineResult
{
    /** 后端实际就绪标志，不能以网络连接成功代替。 */
    bool bReady = false;
    /** 从实际响应解析的契约版本，失败时不得填入配置值冒充响应。 */
    FString ContractVersion;
};

/** 登录或刷新终态；公开快照始终不含原始令牌。 */
struct FGamePlatformOnlineAuthenticationResult : FGamePlatformOnlineResult
{
    /** 本次完成时的认证快照，失败时也需结合 Result 与 State 判断。 */
    FGamePlatformOnlineAuthSnapshot Authentication;
};

/** 资料读取／更新终态；失败时 Profile 不可作为新资料发布。 */
struct FGamePlatformOnlineProfileResult : FGamePlatformOnlineResult
{
    /** 本次操作实际返回的资料；乱序旧响应不得降低服务内部缓存修订。 */
    FGamePlatformOnlineProfile Profile;
};

/** 退出必须区分本地清理与服务器撤销；未执行不能与已退出混淆。 */
enum class EGamePlatformOnlineLogoutDisposition : uint8
{
    NotExecuted,             // 尚未执行本地清理。
    LocalSignedOut,          // 本地已清理，开始时没有可供撤销的会话凭据。
    ServerRevoked,           // 本地已清理且后端明确确认撤销。
    RevocationUnconfirmed    // 本地已清理，但撤销失败、取消、超时或结果不确定。
};

/** 退出的独立完成值；即使 Result 失败，也不撤销已经完成的本地清理。 */
struct FGamePlatformOnlineLogoutResult : FGamePlatformOnlineResult
{
    /** 本地／远端分离结果，不能只凭 Result 是否成功决定本地登录状态。 */
    EGamePlatformOnlineLogoutDisposition Disposition = EGamePlatformOnlineLogoutDisposition::NotExecuted;
};
