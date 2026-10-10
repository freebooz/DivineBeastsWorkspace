// 第一层跨游戏天气数值合同：天气是世界事实，不能包含Niagara、音效或项目英雄身份。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformWeatherTypes.generated.h"

/** 通用天气类型；客户端特效只根据类型和强度选择已注册的表现资产。 */
UENUM(BlueprintType)
enum class EGamePlatformWeatherType : uint8
{
    Clear UMETA(DisplayName="晴天"),
    Cloudy UMETA(DisplayName="阴天"),
    LightRain UMETA(DisplayName="小雨"),
    HeavyRain UMETA(DisplayName="大雨"),
    LightSnow UMETA(DisplayName="小雪"),
    HeavySnow UMETA(DisplayName="大雪"),
    Fog UMETA(DisplayName="雾天"),
    Wind UMETA(DisplayName="大风"),
    Thunderstorm UMETA(DisplayName="雷雨"),
    Blizzard UMETA(DisplayName="暴风雪")
};

/** 服务器目标天气：七个0..1环境参数及摄氏温度，视觉参数不改变服务器移动/伤害。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWEATHERRUNTIME_API FGamePlatformWeatherState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather")
    EGamePlatformWeatherType Type = EGamePlatformWeatherType::Clear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float RainIntensity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float SnowIntensity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float Wetness = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float SnowAmount = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float PuddleAmount = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float FogIntensity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",ClampMax="1"))
    float WindIntensity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="-100",ClampMax="100",Units="Celsius"))
    float TemperatureCelsius = 20.f;

    /** 仅接受有限且范围合法的状态；非法数据拒绝，不静默裁剪或污染权威事实。 */
    bool IsValid() const;
    /** 过渡仅插值连续参数，天气类型在过渡后半段切换，不代表瞬时粒子生命周期。 */
    static FGamePlatformWeatherState Interpolate(const FGamePlatformWeatherState& From, const FGamePlatformWeatherState& To, float Alpha);
};

/** 网络量化天气值；0..255编码0..1，温度采用0.1摄氏度单位；禁止按原生结构大小推断线路字节数。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWEATHERRUNTIME_API FGamePlatformWeatherQuantizedState
{
    GENERATED_BODY()

    UPROPERTY()
    EGamePlatformWeatherType Type = EGamePlatformWeatherType::Clear;
    UPROPERTY()
    uint8 Rain = 0;
    UPROPERTY()
    uint8 Snow = 0;
    UPROPERTY()
    uint8 Wetness = 0;
    UPROPERTY()
    uint8 SnowAmount = 0;
    UPROPERTY()
    uint8 Puddle = 0;
    UPROPERTY()
    uint8 Fog = 0;
    UPROPERTY()
    uint8 Wind = 0;
    UPROPERTY()
    int16 TemperatureTenths = 200;

    /** 输入必须已经通过IsValid；不执行项目资产加载。 */
    static FGamePlatformWeatherQuantizedState Encode(const FGamePlatformWeatherState& State);
    FGamePlatformWeatherState Decode() const;
};

/** 服务器一次完整过渡快照；晚加入者依据GameState服务器时间直接重建当前插值。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWEATHERRUNTIME_API FGamePlatformWeatherSnapshot
{
    GENERATED_BODY()
    UPROPERTY()
    FGamePlatformWeatherQuantizedState From;
    UPROPERTY()
    FGamePlatformWeatherQuantizedState To;
    UPROPERTY()
    float StartedAtServerSeconds = 0.f;
    UPROPERTY()
    float TransitionSeconds = 0.f;
    /** 世界内单调状态版本，0表示尚未激活；网络乱序不回退。 */
    UPROPERTY()
    int32 Revision = 0;

    /** 时间单位秒；零时长直接返回To，所有输入由服务器创建时验证。 */
    FGamePlatformWeatherState Sample(float ServerTimeSeconds) const;
};

/** 一条已解析的天气调度条目；真实Definition由Data租约加载后投影到本值。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWEATHERRUNTIME_API FGamePlatformWeatherScheduleEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather")
    FGamePlatformWeatherState State;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="1",Units="s"))
    float MinHoldSeconds = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="1",Units="s"))
    float MaxHoldSeconds = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="0",Units="s"))
    float TransitionSeconds = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather", meta=(ClampMin="1",ClampMax="1000"))
    int32 Weight = 1;
    /** 校验持续时间及权重；全域配置由服务器可信组合根提交。 */
    bool IsValid() const;
};
