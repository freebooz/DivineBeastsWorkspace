#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformValidationSettings.generated.h"

/**
 * UGamePlatformValidationSettings（游戏平台验证设置）。
 * 平台层只提供通用默认值；DivineBeasts项目层可通过配置覆盖，不要求平台层反向依赖项目代码。
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Game Platform Validation（游戏平台验证）"))
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformValidationSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UGamePlatformValidationSettings();

    /** 资产类名关键字 -> 命名前缀。 */
    UPROPERTY(Config, EditAnywhere, Category="Naming")
    TMap<FString, FString> AssetClassPrefixes;

    /** 已取消旧系统的GameplayTag前缀。 */
    UPROPERTY(Config, EditAnywhere, Category="GameplayTag")
    TArray<FString> RemovedGameplayTagPrefixes;

    /** Server-safe资产禁止依赖的客户端路径关键字。 */
    UPROPERTY(Config, EditAnywhere, Category="Server Safety")
    TArray<FString> ForbiddenServerDependencyPathTokens;

    /** PR/PreSubmit中Warning是否升级为阻断；默认false。 */
    UPROPERTY(Config, EditAnywhere, Category="CI")
    bool bWarningsBlockPreSubmit = false;

    /** Nightly中Warning是否升级为阻断；默认false。 */
    UPROPERTY(Config, EditAnywhere, Category="CI")
    bool bWarningsBlockNightly = false;

    /** Release中Warning是否升级为阻断；默认true。 */
    UPROPERTY(Config, EditAnywhere, Category="CI")
    bool bWarningsBlockRelease = true;
};
