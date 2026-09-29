#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformInteractionSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Interaction"))
class GAMEPLATFORMINTERACTION_API UGamePlatformInteractionSettings final
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Discovery（候选发现）自动刷新间隔，单位秒；仅本地控制端启用。 */
    UPROPERTY(Config, EditAnywhere, Category="Discovery", meta=(ClampMin="0.02"))
    float FocusRefreshInterval = 0.10f;

    /** Hold（长按交互）服务器持续校验间隔，单位秒。 */
    UPROPERTY(Config, EditAnywhere, Category="Sessions", meta=(ClampMin="0.05"))
    float HoldValidationInterval = 0.20f;

    /** 单次 Hold（长按交互）允许配置的最大时长，防止异常配置长期占用目标。 */
    UPROPERTY(Config, EditAnywhere, Category="Sessions", meta=(ClampMin="0.1"))
    float MaxHoldDuration = 30.0f;

    /** 服务端允许的最大交互距离上限，单位厘米。 */
    UPROPERTY(Config, EditAnywhere, Category="Validation", meta=(ClampMin="1.0"))
    float MaxConfiguredInteractionDistance = 600.0f;

    /** 服务器视线检测起点相对权威角色原点允许的最大偏移，单位厘米。 */
    UPROPERTY(Config, EditAnywhere, Category="Validation", meta=(ClampMin="0.0"))
    float MaxTraceOriginOffset = 250.0f;

    /** Begin（开始交互）服务器限流窗口，单位秒。 */
    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="0.1"))
    float BeginRequestWindowSeconds = 1.0f;

    /** Cancel（取消交互）服务器限流窗口，单位秒，与 Begin（开始交互）独立统计。 */
    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="0.1"))
    float CancelRequestWindowSeconds = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="1"))
    int32 MaxBeginRequestsPerWindow = 8;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="1"))
    int32 MaxCancelRequestsPerWindow = 12;

    /** 幂等结果缓存允许保留的最近 Request（请求）数量。 */
    UPROPERTY(Config, EditAnywhere, Category="Dedup", meta=(ClampMin="8"))
    int32 MaxRecentRequests = 128;

    /** 最近 Request（请求）终态缓存的存活时间，单位秒。 */
    UPROPERTY(Config, EditAnywhere, Category="Dedup", meta=(ClampMin="1.0"))
    float RecentRequestLifetimeSeconds = 30.0f;

    /** 本地请求最小间隔，降低误触和无意义 RPC（远程过程调用）突发。 */
    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="0.0"))
    float MinClientRequestInterval = 0.08f;
};
