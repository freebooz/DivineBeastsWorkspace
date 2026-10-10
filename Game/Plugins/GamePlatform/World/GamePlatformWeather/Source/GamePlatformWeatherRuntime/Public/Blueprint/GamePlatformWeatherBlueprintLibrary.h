// 跨项目天气蓝图访问入口；不复制世界子系统，也不向普通客户端暴露权威写入RPC。
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "GamePlatformWeatherBlueprintLibrary.generated.h"

class UGamePlatformWeatherPresetDefinition;

/**
 * UGamePlatformWeatherBlueprintLibrary（平台天气蓝图函数库）。
 *
 * 为第三层项目的GameMode、世界蓝图和开发审核提供统一蓝图入口。
 * 天气状态由GamePlatformWeatherWorldSubsystem唯一拥有，此类不缓存天气、Actor或DataAsset。
 * 非服务器调用写入接口返回false；不创建RPC，不隐式加载资源，不影响战斗属性。
 */
UCLASS()
class GAMEPLATFORMWEATHERRUNTIME_API UGamePlatformWeatherBlueprintLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 读取当前世界插值中的天气状态。尚未激活时返回false并重置输出；单位为服务端世界时间秒。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather", meta=(WorldContext="WorldContextObject"))
    static bool ReadWeather(const UObject* WorldContextObject, FGamePlatformWeatherState& OutWeather);

    /** 蓝图仅可在权威游戏世界调用；客户端返回false，执行前严格校验目标状态。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather", meta=(WorldContext="WorldContextObject"))
    static bool SetWeatherOnAuthority(const UObject* WorldContextObject,
                                     const FGamePlatformWeatherState& TargetWeather,
                                     float TransitionSeconds);

    /** 调用方必须保证Preset已由GamePlatformData加载、校验且租约在调用期间有效。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather", meta=(WorldContext="WorldContextObject"))
    static bool ApplyWeatherPresetOnAuthority(const UObject* WorldContextObject,
                                             const UGamePlatformWeatherPresetDefinition* Preset);
};
