// 通用Gaea兼容地形原生R16/R8→独立Landscape审查地图的Editor Commandlet。
// 只允许/Game/Development/Terrain测试地图；不改生产Village/OpenWorld/竞技世界。
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GamePlatformSurfaceTerrainReviewMapCommandlet.generated.h"

/**
 * 真实UE5.8 Landscape::Import编辑器审核入口。
 * 参数：RawRoot、LayerRoot、LayerIds、LayerAssetPrefix、LayerNamePrefix、MaterialPath、
 * MapPackage。首版校准固定1009×1009，8组件×(2子节×63四边形)+1，要求输入R16与10张R8同网格。
 * 运行时不进入专用服务器，不碰源码Gaea/蓝图/正式地图；失败不创建伪.umap。
 */
UCLASS()
class UGamePlatformSurfaceTerrainReviewMapCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UGamePlatformSurfaceTerrainReviewMapCommandlet();
    /** 成功写入/验证真实UWorld返回0，否则1；ValidateOnly禁止修改任何已存在地图。 */
    virtual int32 Main(const FString& Params) override;
};
