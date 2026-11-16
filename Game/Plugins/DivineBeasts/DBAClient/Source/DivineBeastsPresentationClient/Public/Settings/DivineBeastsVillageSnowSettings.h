// 神兽联盟第三层客户端新手村草地/雪地桥接配置；保留既有Snow反射名以兼容配置。
// 只保存平台雪纹理及项目草地软路径；服务器、共享地图不硬引用客户端高清表现资源。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "DivineBeastsVillageSnowSettings.generated.h"

/**
 * 新手村地形草地与雪层客户端资源入口（历史Snow类型身份不改名）。
 * 默认开启，独立关卡不受影响；加载失败保持地图共享中性材质，不伪造成功。
 * 八张地形贴图共用世界租约；岩石材质单独租约。地图切换时仅释放自身需求。
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

    /** 同一雪扫描的线性ORM：R环境遮蔽、G粗糙度、B金属度；与颜色和法线配套。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    FSoftObjectPath SnowORMTexture;

    /** 同一雪扫描的线性高度，用于受距离限制的近景视差；不改变碰撞或真实地形高度。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Snow")
    FSoftObjectPath SnowHeightTexture;

    /** 项目公共扫描草地基础色，sRGB；保持原图颜色，运行时采用白色乘色。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Ground")
    FSoftObjectPath GroundAlbedoTexture;

    /** 草地切线空间法线，线性NormalMap；不改变地形高度、碰撞或导航。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Ground")
    FSoftObjectPath GroundNormalTexture;

    /** 草地线性ORM：R环境遮蔽、G粗糙度、B金属度（草地固定非金属）。 */
    UPROPERTY(Config, EditAnywhere, Category="Village Ground")
    FSoftObjectPath GroundORMTexture;

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
