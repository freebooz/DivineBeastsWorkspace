#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformWeatherTypes.h"
class UWorld;
class AWindDirectionalSource;

/** 地图初始风配置读取结果；重复或非法源不能按枚举顺序任选其一。 */
enum class EGamePlatformSceneWindResult : uint8 { Absent, Applied, Invalid };
namespace GamePlatformWeatherWindSource
{
    /** 唯一全局风源Actor标签；原生WindDirectionalSource负责初始配置，不另持运行权威。 */
    GAMEPLATFORMWEATHERRUNTIME_API const FName& Tag();
    /** 游戏线程只读；只合并四个风字段，保留雨雪。Speed按本合同解释为0..2Hz，不代表米/秒。
     * 无源返回Absent并保持State；重复、点风、非水平或非法范围返回Invalid，不部分修改。
     */
    GAMEPLATFORMWEATHERRUNTIME_API EGamePlatformSceneWindResult Read(
        UWorld& World, FGamePlatformWeatherState& State, AWindDirectionalSource*& Source, FString& Error);
}
