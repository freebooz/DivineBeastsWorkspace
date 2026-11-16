#pragma once

#include "CoreMinimal.h"

/**
 * GamePlatformSurface（游戏平台环境表面）稳定MPC参数名。
 *
 * 这些名称属于平台公共材质契约，项目材质可引用但不得在项目层重新定义同义全局参数。
 */
namespace GamePlatformSurfaceParameters
{
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalWetness;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalSnowAmount;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalSnowHeightCm;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalMossInfluence;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalPuddleAmount;
    GAMEPLATFORMSURFACECLIENT_API extern const FName RainIntensity;
    GAMEPLATFORMSURFACECLIENT_API extern const FName SnowIntensity;
    GAMEPLATFORMSURFACECLIENT_API extern const FName TemperatureCelsius;
    /** 世界植物风端点/时基为Vector，种子为Scalar；名称与原生MPC合同一致。 */
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalWindFrom;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalWindTo;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalWindTiming;
    GAMEPLATFORMSURFACECLIENT_API extern const FName GlobalWindSeed;
}
