// 世界审核控制Actor：天气状态计算与校验仍归平台层，第三层只选择表现方案。
#include "Weather/DivineBeastsWeatherReviewController.h"

#include "Blueprint/GamePlatformWeatherBlueprintLibrary.h"
#include "Engine/World.h"

ADivineBeastsWeatherReviewController::ADivineBeastsWeatherReviewController()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // 本Actor不参与网络；平台WeatherReplicator才复制天气事实。
}

bool ADivineBeastsWeatherReviewController::ReadCurrentWeather(
    FGamePlatformWeatherState& OutWeather) const
{
    return UGamePlatformWeatherBlueprintLibrary::ReadWeather(this, OutWeather);
}

bool ADivineBeastsWeatherReviewController::ApplyReviewWeather(
    EGamePlatformWeatherType Type, float TransitionDurationSeconds)
{
    if (!HasAuthority() || !IsValid(GetWorld()) || GetWorld()->bIsTearingDown)
        return false;

    FGamePlatformWeatherState Desired;
    Desired.Type = Type;
    switch (Type)
    {
    case EGamePlatformWeatherType::LightRain:
        Desired.RainIntensity = .35f;
        Desired.Wetness = .35f;
        Desired.PuddleAmount = .07f;
        Desired.TemperatureCelsius = 13.f;
        break;
    case EGamePlatformWeatherType::HeavyRain:
        Desired.RainIntensity = .90f;
        Desired.Wetness = .93f;
        Desired.PuddleAmount = .55f;
        Desired.WindIntensity = .3f;
        Desired.TemperatureCelsius = 11.f;
        break;
    case EGamePlatformWeatherType::LightSnow:
        Desired.SnowIntensity = .35f;
        Desired.SnowAmount = .23f;
        Desired.TemperatureCelsius = -2.f;
        break;
    case EGamePlatformWeatherType::HeavySnow:
        Desired.SnowIntensity = .90f;
        Desired.SnowAmount = .85f;
        Desired.WindIntensity = .4f;
        Desired.TemperatureCelsius = -8.f;
        break;
    case EGamePlatformWeatherType::Cloudy:
        Desired.FogIntensity = .18f;
        Desired.TemperatureCelsius = 15.f;
        break;
    case EGamePlatformWeatherType::Clear:
        break;
    default:
        // 该审核控制器首期只包含晴、阴、雨、雪，不冒充其他天气类型已有真实表现资产。
        return false;
    }
    return UGamePlatformWeatherBlueprintLibrary::SetWeatherOnAuthority(
        this, Desired, TransitionDurationSeconds);
}

void ADivineBeastsWeatherReviewController::BeginPlay()
{
    Super::BeginPlay();
    if (!bApplyAtBeginPlay || !HasAuthority()) return;
    if (!ApplyReviewWeather(PreviewWeather, PreviewTransitionSeconds))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("天气审核Actor：权威天气切换失败。检查世界天气激活、预设与服务器阶段。"));
    }
}
