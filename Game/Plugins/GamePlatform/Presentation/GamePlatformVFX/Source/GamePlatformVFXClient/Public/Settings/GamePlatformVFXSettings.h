#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformVFXSettings.generated.h"

class UGamePlatformVFXCatalog;

/** GamePlatformVFX 可配置运行策略。 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform VFX"))
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Combat（战斗）及以上普通软预算；Critical（关键）仅受HardMaxTrackedInstances限制。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxActiveInstances = 256;

    /**
     * Status（状态）及Ambient（环境）的累计软上限。
     * 低优先级请求只能使用预算前段，为Combat预留后段容量。
     */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxStatusInstances = 192;

    /** Ambient（环境）累计软上限；防止开放世界装饰效果抢占战斗预算。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxAmbientInstances = 96;

    /** 绝对安全上限；Critical请求也不得突破。Pending与Active统一计入。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 HardMaxTrackedInstances = 512;

    /** 同时等待共享Definition完成的VFX实例上限。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxPendingInstancePreloads = 64;

    /** World级共享Definition缓存上限；满载时只淘汰无实例、无Preload Pin的最久未使用项。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16", ClampMax="4096"))
    int32 MaxCachedDefinitions = 512;

    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16", ClampMax="4096"))
    int32 MaxDedupeEntries = 512;

    UPROPERTY(Config, EditAnywhere, Category="Catalog", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxRegisteredCatalogs = 64;

    UPROPERTY(Config, EditAnywhere, Category="Catalog", meta=(ClampMin="1", ClampMax="128"))
    int32 MaxStartupCatalogs = 32;

    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bEnablePooling = true;

    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bEnableDiagnostics = true;

    UPROPERTY(Config, EditAnywhere, Category="Catalog")
    TArray<TSoftObjectPtr<UGamePlatformVFXCatalog>> StartupCatalogs;
};
