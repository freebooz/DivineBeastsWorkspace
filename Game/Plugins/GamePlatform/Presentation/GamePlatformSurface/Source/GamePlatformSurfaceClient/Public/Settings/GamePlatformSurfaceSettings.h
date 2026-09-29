#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformSurfaceSettings.generated.h"

class UMaterialParameterCollection;

/**
 * GamePlatformSurface（游戏平台环境表面）客户端配置。
 *
 * 只保存表现资源软引用；不保存天气权威状态、用户凭据或任何服务器业务配置。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Surface"))
class GAMEPLATFORMSURFACECLIENT_API UGamePlatformSurfaceSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UGamePlatformSurfaceSettings();

    /**
     * 全局材质参数集合。
     * 默认指向插件标准资产 /GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal。
     * 资产缺失时运行时保留状态但不伪造成功，编辑器命令可生成该真实uasset。
     */
    UPROPERTY(Config, EditAnywhere, Category="Surface", meta=(AllowedClasses="/Script/Engine.MaterialParameterCollection"))
    TSoftObjectPtr<UMaterialParameterCollection> GlobalParameterCollection;

    /** MPC缺失或参数契约不完整时是否记录一次客户端警告；不影响服务器。 */
    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bWarnOnMaterialBindingFailure = true;
};
