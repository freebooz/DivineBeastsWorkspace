#pragma once

#include "CoreMinimal.h"

/** 非敏感实例配置；由组合根显式传入，不自动读取 INI、环境变量或凭据文件。 */
struct FGamePlatformOnlineConfiguration
{
    /** 网关源地址，仅 scheme://host[:port]，不允许路径、用户信息、查询或片段；不是任意请求 URL。 */
    FString ServiceOrigin;
    /** 公共契约中的游戏身份；平台层不内置具体项目值。 */
    FString GameId;
    /** 调用客户端的构建版本；随登录传递，不是服务契约版本。 */
    FString ClientVersion;
    /** 第一版仅支持 1.0.0；探测必须检查实际响应，不以配置值冒充服务响应。 */
    FString RequiredContractVersion = TEXT("1.0.0");
    /** 必须为 true；false 是配置错误，绝不调用引擎接口关闭证书或主机名验证。 */
    bool bVerifyCertificates = true;
    /** 仅非 Shipping 开发环境显式开启；HTTP 只允许字面回环 127.0.0.1 或 [::1]，不接受私网或 DNS 别名。 */
    bool bAllowLoopbackHttpDevelopment = false;
    /** 单个逻辑请求含排队、刷新等待和重试的总时间上限，秒；默认值是初始策略而非性能实测。 */
    double RequestDeadlineSeconds = 30.0;
    /** 单次网络尝试上限，秒；实际值不超过逻辑请求剩余时间。 */
    double AttemptTimeoutSeconds = 10.0;
    /** 退出撤销的独立短期快照最长存活时间，秒；到期清理，不恢复认证。 */
    double RevocationDeadlineSeconds = 5.0;
    /** 本实例同时进行的网络尝试上限，包括刷新与撤销。 */
    int32 MaxConcurrentRequests = 4;
    /** 本实例等待调度的逻辑请求上限；超出立即进入异步失败完成队列。 */
    int32 MaxQueuedRequests = 32;
    /** 同一认证上下文等待单次刷新结果的请求上限，含显式刷新调用者。 */
    int32 MaxRefreshWaiters = 32;
    /** 单次响应解码前的字节上限；传输期间也必须限制累积大小。 */
    int32 MaxResponseBytes = 262144;
    /** 序列化后的请求正文上限，字节；不包括响应与配置。 */
    int32 MaxRequestBytes = 16384;
    /** 安全读取的额外重试次数，不含首次尝试；登录、轮换刷新不自动重试。 */
    int32 MaxReadRetries = 2;
    /** 安全重试指数退避的基础时长，秒；叠加抖动且受总截止时间约束。 */
    double RetryBaseDelaySeconds = 0.25;
    /** 自动重试单次可接受的最大等待时间，秒；服务提示更长时结束请求，不提前重试。 */
    double MaxRetryDelaySeconds = 5.0;
};
