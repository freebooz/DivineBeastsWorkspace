// GamePlatformPCGEditor（平台PCG编辑器）：Landscape权重过滤Graph通用作者命令。
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GamePlatformPCGTerrainBiomeGraphsCommandlet.generated.h"

/**
 * 读取已有UE5.8 LandscapeLayer权重并按项目指定图层阈值过滤PCG采样点。
 * 只接受显式GraphRoot/GraphPrefix/BiomeMap；平台不硬编码草、泥、神兽联盟路径。
 * 创建的Graph不带任何项目Mesh/Asset路径，也不自动开启生成/保存地图。
 */
UCLASS()
class UGamePlatformPCGTerrainBiomeGraphsCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UGamePlatformPCGTerrainBiomeGraphsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
