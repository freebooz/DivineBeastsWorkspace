// 仅负责数学与输入门禁，不替代GPU Niagara粒子计算及GamePlatformVFX环境预算。
#include "Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h"

bool FGamePlatformBirdFlockSplineMath::EvaluateProgress(
    double ElapsedSeconds, double SpeedCmPerSecond, double PathLengthCm,
    double Phase01, EGamePlatformBirdRouteTraversal Mode,
    double& OutProgress01, bool& bReachedEnd)
{
    OutProgress01 = 0.0;
    bReachedEnd = false;
    if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds < 0.0 ||
        !FMath::IsFinite(SpeedCmPerSecond) || SpeedCmPerSecond <= 0.0 ||
        !FMath::IsFinite(PathLengthCm) || PathLengthCm <= 1.0 ||
        !FMath::IsFinite(Phase01) || Phase01 < 0.0 || Phase01 >= 1.0)
    {
        return false;
    }
    const double Normalized = Phase01 + ElapsedSeconds * SpeedCmPerSecond / PathLengthCm;
    if (!FMath::IsFinite(Normalized)) return false;
    switch (Mode)
    {
    case EGamePlatformBirdRouteTraversal::Once:
        bReachedEnd = Normalized >= 1.0;
        OutProgress01 = FMath::Clamp(Normalized, 0.0, 1.0);
        break;
    case EGamePlatformBirdRouteTraversal::Loop:
        OutProgress01 = FMath::Fmod(Normalized, 1.0);
        break;
    case EGamePlatformBirdRouteTraversal::PingPong:
    {
        const double Phase = FMath::Fmod(Normalized, 2.0);
        OutProgress01 = Phase <= 1.0 ? Phase : 2.0 - Phase;
        break;
    }
    default:
        return false;
    }
    return FMath::IsFinite(OutProgress01);
}

bool FGamePlatformBirdFlockSplineMath::ValidateFlockingWeights(
    float Separation, float Alignment, float Cohesion)
{
    return FMath::IsFinite(Separation) && Separation >= 0.0f && Separation <= 4.0f &&
        FMath::IsFinite(Alignment) && Alignment >= 0.0f && Alignment <= 4.0f &&
        FMath::IsFinite(Cohesion) && Cohesion >= 0.0f && Cohesion <= 4.0f &&
        Separation + Alignment + Cohesion <= 4.0f;
}

int32 FGamePlatformBirdFlockSplineMath::ResolveVisibleCount(int32 RequestedCount, float QualityFraction)
{
    if (RequestedCount < 0 || RequestedCount > 512 ||
        !FMath::IsFinite(QualityFraction) || QualityFraction < 0.f || QualityFraction > 1.f)
    {
        return INDEX_NONE;
    }
    return FMath::Clamp(FMath::RoundToInt(RequestedCount * QualityFraction), 0, 512);
}
