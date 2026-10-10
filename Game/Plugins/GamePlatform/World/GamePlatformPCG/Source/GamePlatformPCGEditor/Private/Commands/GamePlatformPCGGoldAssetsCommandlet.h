#pragma once

#include "Commandlets/Commandlet.h"
#include "GamePlatformPCGGoldAssetsCommandlet.generated.h"

/**
 * PCG金标准开发资产命令行工具。
 * 只在编辑器目标中创建预先批准的Definition（数据定义）及Realized Graph（真实网格图）。
 * 不运行PCG、不编辑真实Village地图、不替代工程自动化测试或发布审核。
 */
UCLASS()
class UGamePlatformPCGGoldAssetsCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UGamePlatformPCGGoldAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
