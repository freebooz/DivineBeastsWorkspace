#pragma once

#include "Types/GamePlatformOnlineResult.h"

/** 服务就绪状态与认证状态分离，不以失联事件自动清除认证上下文。 */
enum class EGamePlatformOnlineServiceState : uint8
{
    Unknown,       // 尚未进行有效探测。
    Ready,         // 最近实际探测确认就绪且兼容。
    Unavailable,   // 最近探测不可达或依赖未就绪。
    Incompatible   // 最近探测响应的服务身份或版本不兼容。
};

/** 只读脱敏实例诊断；不含账号名、PlayerId、地址、报文、请求头和凭据。 */
struct FGamePlatformOnlineDiagnostics
{
    /** 当前游戏实例服务作用域。 */
    FGuid InstanceScopeId;
    /** 是否已经接受合法配置；不意味着服务可达。 */
    bool bConfigured = false;
    /** 最近一次真实探测的结果状态。 */
    EGamePlatformOnlineServiceState ServiceState = EGamePlatformOnlineServiceState::Unknown;
    /** 当前认证状态，不包含认证主体。 */
    EGamePlatformOnlineAuthState AuthState = EGamePlatformOnlineAuthState::SignedOut;
    /** 当前进行中的网络尝试数。 */
    int32 ActiveRequests = 0;
    /** 当前等待调度的逻辑请求数。 */
    int32 QueuedRequests = 0;
    /** 当前共享刷新等待者数。 */
    int32 RefreshWaiters = 0;
    /** 本实例累计实际启动的刷新数，可用于验证 single flight。 */
    uint64 RefreshAttempts = 0;
    /** 本实例累计进入一次性终态的逻辑请求数。 */
    uint64 CompletedRequests = 0;
    /** 最近一次终态的安全分类，不保留原始远端消息。 */
    EGamePlatformOnlineError LastError = EGamePlatformOnlineError::None;
};
