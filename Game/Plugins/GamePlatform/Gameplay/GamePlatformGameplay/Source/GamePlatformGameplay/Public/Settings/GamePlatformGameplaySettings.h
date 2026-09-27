#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformGameplaySettings.generated.h"

/**
 * 通用玩法安全上限；资产可以在范围内收紧，不能扩大到无界等待。
 * 配置不包含端点、令牌、密钥或生产账号。运行期只读，修改后需重启进程。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Gameplay"))
class GAMEPLATFORMGAMEPLAY_API UGamePlatformGameplaySettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 单个世界允许等待的玩家硬上限；1..1024，默认128。 */
    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaximumWaitingPlayers = 128;

    /** 任一体验或玩家等待阶段允许的最大秒数；1..600，默认120。 */
    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="1.0", ClampMax="600.0", Units="s"))
    float MaximumPhaseTimeoutSeconds = 120.f;

    /** 同一连接准备报告的最小间隔秒数；0.01..5，默认0.1，用于抑制超频而非认证。 */
    UPROPERTY(Config, EditAnywhere, Category="Networking", meta=(ClampMin="0.01", ClampMax="5.0", Units="s"))
    float MinimumPreparationReportIntervalSeconds = 0.1f;

    /** 单个世界最多注册的出生策略数；1..128，默认32。 */
    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="1", ClampMax="128"))
    int32 MaximumSpawnPolicies = 32;

    /** 单次策略最多返回的出生候选；1..1024，默认128，超出直接拒绝本次结果。 */
    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaximumSpawnCandidates = 128;

    /** 纯字段校验；失败返回稳定错误码，不自动修复越界配置。 */
    FGamePlatformResult ValidateSettings() const;
};
