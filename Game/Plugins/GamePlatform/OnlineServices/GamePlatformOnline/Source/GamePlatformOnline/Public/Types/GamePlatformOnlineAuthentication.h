#pragma once

#include "CoreMinimal.h"

/** 认证状态与服务可达性正交；网络失败不能擅自判定服务端会话已撤销。 */
enum class EGamePlatformOnlineAuthState : uint8
{
    /** 本地没有可使用的玩家认证。 */
    SignedOut,
    /** 单个登录正在进行，拒绝并发第二账号。 */
    SigningIn,
    /** 已认证；实际令牌有效期另见快照。 */
    SignedIn,
    /** 本上下文唯一刷新进行中；其他合法请求有界等待。 */
    Refreshing,
    /** 轮换结果不确定或凭据失效，需要显式退出后重新登录；不得盲重发旧刷新令牌。 */
    ReauthenticationRequired
};

/** 不含密码或原始令牌的认证值快照；仅游戏线程读取，调用者自行决定展示时的玩家标识脱敏。 */
struct FGamePlatformOnlineAuthSnapshot
{
    /** 默认未登录，不将服务探测成功当作认证成功。 */
    EGamePlatformOnlineAuthState State = EGamePlatformOnlineAuthState::SignedOut;
    /** 本次本地认证上下文身份，退出即失效；不是服务端会话凭据。 */
    FGuid AuthContextId;
    /** 登录、退出及失效化时变化，用于隔离迟到响应。 */
    uint64 AuthGeneration = 0;
    /** 本上下文成功采纳的令牌版本；仅版本号，绝不提供令牌原文。 */
    uint64 TokenVersion = 0;
    /** 已认证玩家的公共标识；未登录时为空。 */
    FString PlayerId;
    /** 访问令牌 UTC 到期时间；默认零值表示未取得，不代表永久有效。 */
    FDateTime AccessExpiresAt;
    /** 刷新令牌 UTC 到期时间；不构成客户端自行延长授权的依据。 */
    FDateTime RefreshExpiresAt;
};
