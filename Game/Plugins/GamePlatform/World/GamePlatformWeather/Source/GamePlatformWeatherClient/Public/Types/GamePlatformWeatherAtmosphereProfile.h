// 客户端天气光雾参数：项目选择数值，平台不认识具体世界；所有颜色为UE线性值。
#pragma once
#include "CoreMinimal.h"

/** 一个世界仅由既有WeatherClient持有一份；禁用时恢复捕获的场景基线，不写入地图资产。 */
struct GAMEPLATFORMWEATHERCLIENT_API FGamePlatformWeatherAtmosphereProfile
{
    /** 默认关闭，项目组合根明确选择需要参与天气的世界。 */
    bool bEnabled = false;
    /** 满阴云时方向光/天光相对原场景的倍率，保留环境可读性。 */
    float OvercastSunMultiplier = .25f;
    float OvercastSkyMultiplier = .72f;
    /** 高度雾增量及最近清晰范围（厘米）；不启用额外体积雾或天空实时重捕获。 */
    float AdditionalFogDensity = .025f;
    float FogStartDistanceCm = 350.f;
    FLinearColor LightTint = FLinearColor(.82f, .89f, 1.f);
    FLinearColor FogColor = FLinearColor(.42f, .50f, .61f);
};
