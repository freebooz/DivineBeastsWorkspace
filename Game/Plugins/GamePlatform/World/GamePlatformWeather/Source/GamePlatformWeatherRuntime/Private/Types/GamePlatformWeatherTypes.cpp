// 通用天气数值实现；量化误差只用于客户端表现，不进入伤害等权威规则。
#include "Types/GamePlatformWeatherTypes.h"

namespace
{
bool IsRatioValid(const float Value)
{
    return FMath::IsFinite(Value) && Value >= 0.f && Value <= 1.f;
}
uint8 EncodeRatio(const float Value)
{
    return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value * 255.f), 0, 255));
}
float DecodeRatio(const uint8 Value)
{
    return static_cast<float>(Value) / 255.f;
}
}

bool FGamePlatformWeatherState::IsValid() const
{
    return static_cast<uint8>(Type) <= static_cast<uint8>(EGamePlatformWeatherType::Blizzard) &&
        IsRatioValid(RainIntensity) && IsRatioValid(SnowIntensity) &&
        IsRatioValid(Wetness) && IsRatioValid(SnowAmount) && IsRatioValid(PuddleAmount) &&
        IsRatioValid(FogIntensity) && IsRatioValid(WindIntensity) && IsRatioValid(WindGustStrength) &&
        FMath::IsFinite(WindYawDegrees) && WindYawDegrees >= 0.f && WindYawDegrees < 360.f &&
        FMath::IsFinite(WindGustFrequencyHz) && WindGustFrequencyHz >= 0.f && WindGustFrequencyHz <= 2.f &&
        FMath::IsFinite(TemperatureCelsius) && TemperatureCelsius >= -100.f && TemperatureCelsius <= 100.f;
}

FGamePlatformWeatherState FGamePlatformWeatherState::Interpolate(
    const FGamePlatformWeatherState& From, const FGamePlatformWeatherState& To, float Alpha)
{
    const float Fraction = FMath::Clamp(Alpha, 0.f, 1.f);
    FGamePlatformWeatherState Result;
    Result.Type = Fraction >= .5f ? To.Type : From.Type;
    Result.RainIntensity = FMath::Lerp(From.RainIntensity, To.RainIntensity, Fraction);
    Result.SnowIntensity = FMath::Lerp(From.SnowIntensity, To.SnowIntensity, Fraction);
    Result.Wetness = FMath::Lerp(From.Wetness, To.Wetness, Fraction);
    Result.SnowAmount = FMath::Lerp(From.SnowAmount, To.SnowAmount, Fraction);
    Result.PuddleAmount = FMath::Lerp(From.PuddleAmount, To.PuddleAmount, Fraction);
    Result.FogIntensity = FMath::Lerp(From.FogIntensity, To.FogIntensity, Fraction);
    Result.WindIntensity = FMath::Lerp(From.WindIntensity, To.WindIntensity, Fraction);
    Result.WindYawDegrees = FMath::Fmod(From.WindYawDegrees +
        FMath::FindDeltaAngleDegrees(From.WindYawDegrees, To.WindYawDegrees) * Fraction + 360.f, 360.f);
    Result.WindGustStrength = FMath::Lerp(From.WindGustStrength, To.WindGustStrength, Fraction);
    Result.WindGustFrequencyHz = FMath::Lerp(From.WindGustFrequencyHz, To.WindGustFrequencyHz, Fraction);
    Result.TemperatureCelsius = FMath::Lerp(From.TemperatureCelsius, To.TemperatureCelsius, Fraction);
    return Result;
}

FGamePlatformWeatherQuantizedState FGamePlatformWeatherQuantizedState::Encode(const FGamePlatformWeatherState& State)
{
    check(State.IsValid());
    FGamePlatformWeatherQuantizedState Result;
    Result.Type = State.Type;
    Result.Rain = EncodeRatio(State.RainIntensity);
    Result.Snow = EncodeRatio(State.SnowIntensity);
    Result.Wetness = EncodeRatio(State.Wetness);
    Result.SnowAmount = EncodeRatio(State.SnowAmount);
    Result.Puddle = EncodeRatio(State.PuddleAmount);
    Result.Fog = EncodeRatio(State.FogIntensity);
    Result.Wind = EncodeRatio(State.WindIntensity);
    Result.WindYaw = static_cast<uint16>(FMath::RoundToInt(State.WindYawDegrees * (65536.f / 360.f)) & 65535);
    Result.WindGust = EncodeRatio(State.WindGustStrength);
    Result.WindGustMilliHz = static_cast<uint16>(FMath::RoundToInt(State.WindGustFrequencyHz * 1000.f));
    Result.TemperatureTenths = static_cast<int16>(FMath::RoundToInt(State.TemperatureCelsius * 10.f));
    return Result;
}

FGamePlatformWeatherState FGamePlatformWeatherQuantizedState::Decode() const
{
    FGamePlatformWeatherState Result;
    Result.Type = Type;
    Result.RainIntensity = DecodeRatio(Rain);
    Result.SnowIntensity = DecodeRatio(Snow);
    Result.Wetness = DecodeRatio(Wetness);
    Result.SnowAmount = DecodeRatio(SnowAmount);
    Result.PuddleAmount = DecodeRatio(Puddle);
    Result.FogIntensity = DecodeRatio(Fog);
    Result.WindIntensity = DecodeRatio(Wind);
    Result.WindYawDegrees = static_cast<float>(WindYaw) * (360.f / 65536.f);
    Result.WindGustStrength = DecodeRatio(WindGust);
    Result.WindGustFrequencyHz = static_cast<float>(WindGustMilliHz) / 1000.f;
    Result.TemperatureCelsius = static_cast<float>(TemperatureTenths) / 10.f;
    return Result;
}

FGamePlatformWeatherState FGamePlatformWeatherSnapshot::Sample(float ServerTimeSeconds) const
{
    const FGamePlatformWeatherState Destination = To.Decode();
    if (Revision <= 0 || TransitionSeconds <= 0.f) return Destination;
    const float Alpha = FMath::Clamp((ServerTimeSeconds - StartedAtServerSeconds) / TransitionSeconds, 0.f, 1.f);
    return FGamePlatformWeatherState::Interpolate(From.Decode(), Destination, Alpha);
}

float FGamePlatformWeatherSnapshot::SampleWindPhaseCycles(const float ServerTimeSeconds) const
{
    // 使用double积分，避免长时间运行后float乘积丢失周期小数。
    const double Time = FMath::Max(0.0, static_cast<double>(ServerTimeSeconds) - StartedAtServerSeconds);
    const double Duration = TransitionSeconds;
    const double F0 = From.Decode().WindGustFrequencyHz, F1 = To.Decode().WindGustFrequencyHz;
    const double Integral = Duration <= 0.0 ? F1 * Time : Time < Duration
        ? F0 * Time + (F1 - F0) * Time * Time / (2.0 * Duration)
        : (F0 + F1) * Duration * .5 + F1 * (Time - Duration);
    return static_cast<float>(FMath::Fmod(Integral + WindPhaseAtStartCycles, 1.0));
}

bool FGamePlatformWeatherScheduleEntry::IsValid() const
{
    return State.IsValid() && FMath::IsFinite(MinHoldSeconds) && FMath::IsFinite(MaxHoldSeconds) &&
        FMath::IsFinite(TransitionSeconds) && MinHoldSeconds >= 1.f &&
        MaxHoldSeconds >= MinHoldSeconds && MaxHoldSeconds <= 86400.f &&
        TransitionSeconds >= 0.f && TransitionSeconds <= 3600.f &&
        Weight >= 1 && Weight <= 1000;
}
