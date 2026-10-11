// 通用Landscape LayerInfo真UE资产制作入口，归GamePlatformSurfaceEditor（平台编辑器层）。
// 用户命令参数指定目标项目内容包与Layer名字，平台不认识桃林/神兽联盟；不进入客户端/专服。
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GamePlatformSurfaceTerrainLayersCommandlet.generated.h"

/**
 * UGamePlatformSurfaceTerrainLayersCommandlet（通用地形图层信息创作命令）。
 * 支持显式LayerRoot、LayerIds、LayerNamePrefix及AssetPrefix；
 * bValidateOnly时仅加载已存在真实LandscapeLayerInfoObject并检查其LayerName/BlendMethod。
 * 唯有编辑器目标允许创建和保存，目标已存在但合同不匹配一律拒绝覆盖。
 */
UCLASS()
class UGamePlatformSurfaceTerrainLayersCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UGamePlatformSurfaceTerrainLayersCommandlet();

    /** 返回0=全部创建或验证成功，1=目标非法/图层缺失/保存失败；参数必须由调用方显式给出。 */
    virtual int32 Main(const FString& Params) override;
};
