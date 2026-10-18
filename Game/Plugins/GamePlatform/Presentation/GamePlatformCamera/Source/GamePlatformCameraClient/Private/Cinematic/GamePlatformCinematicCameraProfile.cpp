// 配置验证与降级：拒绝非有限数值，杜绝坏资产污染镜头。
#include "Cinematic/GamePlatformCinematicCameraProfile.h"

float UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(
    float FocalLengthMm, float SensorWidthMm)
{
    if (!FMath::IsFinite(FocalLengthMm) || !FMath::IsFinite(SensorWidthMm) ||
        FocalLengthMm <= 0.0f || SensorWidthMm <= 0.0f)
    {
        return 0.0f;
    }

    const float FieldOfViewRadians =
        2.0f * FMath::Atan(SensorWidthMm / (2.0f * FocalLengthMm));
    return FMath::RadiansToDegrees(FieldOfViewRadians);
}

bool UGamePlatformCinematicCameraProfile::IsValidProfile() const
{
    return FMath::IsFinite(SensorWidthMm) && SensorWidthMm >= 12.f && SensorWidthMm <= 70.f &&
        FMath::IsFinite(FocalLengthMm) && FocalLengthMm >= 12.f && FocalLengthMm <= 200.f &&
        FMath::IsFinite(Aperture) && Aperture >= 1.2f && Aperture <= 22.f &&
        FMath::IsFinite(FocusDistanceCm) && FocusDistanceCm >= 10.f && FocusDistanceCm <= 100000.f &&
        FMath::IsFinite(BlendTimeSeconds) && BlendTimeSeconds >= 0.f && BlendTimeSeconds <= 5.f;
}
