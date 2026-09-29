// 表面环境状态的纯数据校验与归一化实现；不访问世界、材质或网络状态。
#include "Types/GamePlatformSurfaceTypes.h"

bool FGamePlatformSurfaceEnvironmentState::IsFinite() const
{
    return FMath::IsFinite(GlobalWetness) &&
           FMath::IsFinite(GlobalSnowAmount) &&
           FMath::IsFinite(GlobalSnowHeightCm) &&
           FMath::IsFinite(GlobalMossInfluence) &&
           FMath::IsFinite(GlobalPuddleAmount) &&
           FMath::IsFinite(RainIntensity) &&
           FMath::IsFinite(SnowIntensity) &&
           FMath::IsFinite(TemperatureCelsius);
}

FGamePlatformSurfaceEnvironmentState FGamePlatformSurfaceEnvironmentState::GetClamped() const
{
    FGamePlatformSurfaceEnvironmentState Result = *this;
    Result.GlobalWetness = FMath::Clamp(Result.GlobalWetness, 0.0f, 1.0f);
    Result.GlobalSnowAmount = FMath::Clamp(Result.GlobalSnowAmount, 0.0f, 1.0f);
    // Large World Coordinates 下保留足够范围，同时阻止异常值造成材质精度灾难。
    Result.GlobalSnowHeightCm = FMath::Clamp(Result.GlobalSnowHeightCm, -100000000.0f, 100000000.0f);
    Result.GlobalMossInfluence = FMath::Clamp(Result.GlobalMossInfluence, 0.0f, 1.0f);
    Result.GlobalPuddleAmount = FMath::Clamp(Result.GlobalPuddleAmount, 0.0f, 1.0f);
    Result.RainIntensity = FMath::Clamp(Result.RainIntensity, 0.0f, 1.0f);
    Result.SnowIntensity = FMath::Clamp(Result.SnowIntensity, 0.0f, 1.0f);
    Result.TemperatureCelsius = FMath::Clamp(Result.TemperatureCelsius, -100.0f, 100.0f);
    return Result;
}

bool FGamePlatformSurfaceEnvironmentState::IsNearlyEqual(
    const FGamePlatformSurfaceEnvironmentState& Other,
    const float Tolerance) const
{
    return FMath::IsNearlyEqual(GlobalWetness, Other.GlobalWetness, Tolerance) &&
           FMath::IsNearlyEqual(GlobalSnowAmount, Other.GlobalSnowAmount, Tolerance) &&
           FMath::IsNearlyEqual(GlobalSnowHeightCm, Other.GlobalSnowHeightCm, Tolerance) &&
           FMath::IsNearlyEqual(GlobalMossInfluence, Other.GlobalMossInfluence, Tolerance) &&
           FMath::IsNearlyEqual(GlobalPuddleAmount, Other.GlobalPuddleAmount, Tolerance) &&
           FMath::IsNearlyEqual(RainIntensity, Other.RainIntensity, Tolerance) &&
           FMath::IsNearlyEqual(SnowIntensity, Other.SnowIntensity, Tolerance) &&
           FMath::IsNearlyEqual(TemperatureCelsius, Other.TemperatureCelsius, Tolerance);
}

bool FGamePlatformSurfaceUpdateResult::IsStateAccepted() const
{
    return Status == EGamePlatformSurfaceUpdateStatus::Applied ||
           Status == EGamePlatformSurfaceUpdateStatus::Unchanged ||
           Status == EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable ||
           Status == EGamePlatformSurfaceUpdateStatus::ParameterContractMismatch;
}
