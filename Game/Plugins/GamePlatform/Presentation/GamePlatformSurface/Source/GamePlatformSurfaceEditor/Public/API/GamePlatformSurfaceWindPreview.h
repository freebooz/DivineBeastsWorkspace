#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformSurfaceTypes.h"
class UWorld;

/** 只读/写Editor世界的MPC实例；不改资产默认值，不进入PIE/Game，也不拥有天气状态。 */
namespace GamePlatformSurfaceWindPreview
{
    GAMEPLATFORMSURFACEEDITOR_API bool Read(UWorld& World, FGamePlatformSurfaceWindTransition& Wind, FString& Error);
    /** 原子预检后仅写风四项，保留雨雪等八项；失败不部分写入。 */
    GAMEPLATFORMSURFACEEDITOR_API bool Apply(UWorld& World, const FGamePlatformSurfaceWindTransition& Wind, FString& Error);
}
