// 平台公共MPC参数名唯一实现，避免多个调用方维护同义字符串。
#include "Types/GamePlatformSurfaceParameterNames.h"

namespace GamePlatformSurfaceParameters
{
    const FName GlobalWetness(TEXT("GP_Surface_GlobalWetness"));
    const FName GlobalSnowAmount(TEXT("GP_Surface_GlobalSnowAmount"));
    const FName GlobalSnowHeightCm(TEXT("GP_Surface_GlobalSnowHeightCm"));
    const FName GlobalMossInfluence(TEXT("GP_Surface_GlobalMossInfluence"));
    const FName GlobalPuddleAmount(TEXT("GP_Surface_GlobalPuddleAmount"));
    const FName RainIntensity(TEXT("GP_Surface_RainIntensity"));
    const FName SnowIntensity(TEXT("GP_Surface_SnowIntensity"));
    const FName TemperatureCelsius(TEXT("GP_Surface_TemperatureCelsius"));
}
