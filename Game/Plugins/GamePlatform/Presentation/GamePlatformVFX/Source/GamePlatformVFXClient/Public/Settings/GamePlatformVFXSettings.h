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
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxActiveInstances = 256;

    /** 绝对安全上限；Critical请求也不得突破。Pending与Active统一计入。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 HardMaxTrackedInstances = 512;

    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxPendingInstancePreloads = 64;

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
