#pragma once

#include "Engine/DeveloperSettings.h"
#include "GamePlatformSettingsProjectSettings.generated.h"

/**
 * 游戏平台设置工程级策略。
 * 仅控制基础设施容量、迁移版本和严格模式，不存放项目玩法配置或玩家偏好。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="游戏平台设置"))
class GAMEPLATFORMSETTINGSRUNTIME_API UGamePlatformSettingsProjectSettings
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 当前用户设置档案 Schema 版本；只允许单步 Migration 链升级。 */
    UPROPERTY(Config, EditAnywhere, Category="版本", meta=(ClampMin="1", ClampMax="1024"))
    int32 SchemaVersion = 1;

    /** 严格模式下 Provider/持久层冲突或非法值使 Reload 失败并回退默认值。 */
    UPROPERTY(Config, EditAnywhere, Category="校验")
    bool bStrictValidation = true;

    /** 单个 GameInstance 最多 Provider 数量。 */
    UPROPERTY(Config, EditAnywhere, Category="容量", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxProviders = 64;

    /** 单个 GameInstance 最多 Descriptor 数量。 */
    UPROPERTY(Config, EditAnywhere, Category="容量", meta=(ClampMin="1", ClampMax="8192"))
    int32 MaxDescriptors = 1024;

    /** 单个 GameInstance 最多状态订阅数。 */
    UPROPERTY(Config, EditAnywhere, Category="容量", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxSubscriptions = 128;

    /** 客户端本地 User Profile 默认 SaveGame 槽；项目层可通过配置覆盖，不硬编码账号信息。 */
    UPROPERTY(Config, EditAnywhere, Category="持久化")
    FString LocalProfileSlotName = TEXT("GamePlatformSettings");

    virtual FName GetCategoryName() const override
    {
        return FName(TEXT("GamePlatform"));
    }
};
