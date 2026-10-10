#pragma once

#include "CoreMinimal.h"

/**
 * 金标准PCG地图原生创作/独立重开入口。
 * 仅UE5.8编辑器模块调用，不修改正式Village地图；不执行PCG Generate/Cook。
 */
namespace GamePlatformPCGGoldMap
{
    bool Create(FString& Error);
    bool Verify(FString& Error);
}
