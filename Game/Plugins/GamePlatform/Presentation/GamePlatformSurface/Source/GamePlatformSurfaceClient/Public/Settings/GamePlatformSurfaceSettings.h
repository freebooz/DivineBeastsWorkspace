#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformSurfaceSettings.generated.h"

class UMaterialParameterCollection;
class UGamePlatformSurfaceMarkCatalog;
class UMaterialInterface;

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

    /** 局部痕迹可选目录；仅普通客户端软资源，由Data租约异步加载，服务器完全忽略。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|Marks")
    TSoftObjectPtr<UGamePlatformSurfaceMarkCatalog> MarkCatalog;

    /** 允许项目关闭所有局部痕迹，避免移动端过量Decal；不会改变命中判定。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|Marks")
    bool bEnableSurfaceMarks = true;

    /** MPC缺失或参数契约不完整时是否记录一次客户端警告；不影响服务器。 */
    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bWarnOnMaterialBindingFailure = true;

    /** 可选压雪功能由项目启用；平台默认关闭，不影响没有雪的游戏。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|SnowContacts") bool bEnableSnowContacts = false;
    /** 通过Data世界租约异步加载的批量更新材质；不允许服务器依赖。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|SnowContacts") TSoftObjectPtr<UMaterialInterface> SnowContactUpdateMaterial;
    /** 高档纹理分辨率，安全范围256—1024；两张RG16f在1024时合计8MiB数据。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|SnowContacts") int32 SnowContactResolution = 1024;
    /** 单世界局部滚动窗口边长，厘米，默认16米。 */
    UPROPERTY(Config, EditAnywhere, Category="Surface|SnowContacts") float SnowContactExtentCm = 1600.f;
};
