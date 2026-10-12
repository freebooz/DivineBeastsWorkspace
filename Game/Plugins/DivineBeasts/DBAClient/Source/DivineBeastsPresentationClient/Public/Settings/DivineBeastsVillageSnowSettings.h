// 神兽联盟第三层客户端新手村雪地桥接配置。
// 本配置仅保存跨游戏Surface Texture2D的软路径；服务器、共享地图均不硬引用客户端表现资源。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "DivineBeastsVillageSnowSettings.generated.h"

/**
 * 新手村地形雪层客户端资源入口。
 * 默认开启，独立关卡不受影响；加载失败保持地图共享中性材质，不伪造成功。
 * 四张资源只在客户端当前世界以GamePlatformData租约异步加载，地图切换时释放。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Divine Beasts Village Snow"))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsVillageSnowSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UDivineBeastsVillageSnowSettings();

    /** 开发或发布客户端是否在真正的Village世界绑定雪纹理；不会改变服务器权威或碰撞。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    bool bEnableVillageSnowPresentation = true;

    /** 纯客户端宏观积雪遮罩，线性灰度贴图。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    FSoftObjectPath SnowCoverageTexture;

    /** 纯客户端写实积雪基础颜色，sRGB贴图。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    FSoftObjectPath SnowAlbedoTexture;

    /** 纯客户端切线空间积雪法线贴图。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    FSoftObjectPath SnowNormalTexture;

    /** 是否在Village客户端对明确打标签的岩石网格应用雪苔材质；不会扫描并覆盖未授权的场景表面。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Moss")
    bool bEnableTaggedMossSnowRocks = true;

    /** 只有带此ComponentTag（组件标记）的网格材质才允许被替换，避免破坏其他插件的表现资源。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Moss")
    FName MossSnowComponentTag = TEXT("DBA.Surface.MossSnow");

    /** 世界期异步加载的项目层材质实例软路径；不允许共享服务器地图硬引用客户端纹理。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Moss")
    FSoftObjectPath MossSnowRockMaterial;
};
