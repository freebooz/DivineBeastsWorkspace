// 平台Editor命名与验证配置：只读资产反射事实，供DataValidation/CI调用；项目通过配置覆盖，无运行时资源所有权。
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

    /**
     * 资产类型关键字或蓝图父类身份 -> 命名前缀；空前缀表示该规则不要求前缀。
     * AnimBlueprint/WidgetBlueprint优先按资产类型匹配；普通蓝图按最近父类祖先的完整反射路径或精确类名匹配，
     * 同一祖先完整路径优先；未命中父类规则时使用Blueprint。其他资产沿用固定顺序的类型关键字规则。
     * 配置只影响命名审核，不更改资产身份、父类、挂载点或资源引用；禁止依赖TMap遍历顺序决定优先级。
     */
    UPROPERTY(Config, EditAnywhere, Category="Naming")
    TMap<FString, FString> AssetClassPrefixes;

    /**
     * 游戏线程读取已加载资产与蓝图ParentClass祖先链，返回本配置要求的前缀；空对象或无规则返回空字符串。
     * 不加载项目/GAS资源、不缓存UClass、不修改对象；领域身份由真实继承确定，不能由资产名称猜测。
     */
    FString ResolveAssetPrefix(const UObject* Asset) const;

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
