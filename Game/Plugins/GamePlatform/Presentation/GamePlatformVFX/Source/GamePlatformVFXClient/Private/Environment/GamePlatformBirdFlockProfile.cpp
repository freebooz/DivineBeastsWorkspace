// GamePlatform平台层鸟群预设校验及数值边界；所有鸟群请求在客户端本地完成检查。
#include "Environment/GamePlatformBirdFlockProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

bool UGamePlatformBirdFlockProfile::Validate(FString& OutReason) const
{
    OutReason.Empty();
    if (!BirdMesh || !BirdMaterial)
    {
        OutReason = TEXT("鸟群预设必须同时指定真实静态网格与材质实例");
        return false;
    }
    if (DefaultBirdCount < 0 || MaxBirdCount < 0 ||
        MaxBirdCount > FGamePlatformBirdFlockMath::HardMaxBirdCount ||
        DefaultBirdCount > MaxBirdCount)
    {
        OutReason = TEXT("鸟类数量必须在0..512之间且默认数量不能超过物种上限");
        return false;
    }
    if (!FMath::IsFinite(RadiusCm) || RadiusCm < 200.0f || RadiusCm > 100000.0f ||
        !FMath::IsFinite(FlightHeightCm) || FlightHeightCm < 0.0f || FlightHeightCm > 100000.0f ||
        !FMath::IsFinite(CruiseSpeedCmPerSecond) || CruiseSpeedCmPerSecond < 10.0f ||
        CruiseSpeedCmPerSecond > 5000.0f ||
        !FMath::IsFinite(HeightVariationCm) || HeightVariationCm < 0.0f ||
        HeightVariationCm > 2000.0f ||
        !FMath::IsFinite(WindInfluenceCm) || WindInfluenceCm < 0.0f ||
        WindInfluenceCm > 2000.0f ||
        !FMath::IsFinite(UpdateRateHz) || UpdateRateHz < 5.0f || UpdateRateHz > 60.0f)
    {
        OutReason = TEXT("鸟群飞行参数存在非有限数值、无效半径/速度/高度或不合法更新频率");
        return false;
    }
    if (MinDrawDistanceCm < 0 || MaxDrawDistanceCm <= MinDrawDistanceCm)
    {
        OutReason = TEXT("鸟群裁剪距离范围必须严格递增且非负");
        return false;
    }
    return true;
}

int32 FGamePlatformBirdFlockMath::ResolveBirdCount(
    const UGamePlatformBirdFlockProfile& Profile, int32 CountOverride, float QualityFraction)
{
    if (CountOverride < -1 || CountOverride > HardMaxBirdCount ||
        !FMath::IsFinite(QualityFraction) || QualityFraction < 0.0f || QualityFraction > 1.0f ||
        Profile.MaxBirdCount < 0 || Profile.MaxBirdCount > HardMaxBirdCount ||
        Profile.DefaultBirdCount < 0 || Profile.DefaultBirdCount > Profile.MaxBirdCount)
    {
        return INDEX_NONE;
    }

    const int32 Requested = CountOverride == -1 ? Profile.DefaultBirdCount : CountOverride;
    return FMath::FloorToInt(FMath::Min(Requested, Profile.MaxBirdCount) * QualityFraction);
}

float FGamePlatformBirdFlockMath::SeedToUnit(int32 RandomSeed, int32 BirdIndex, uint32 Salt)
{
    // 固定32位散列只用于视觉分布，不能用于认证/网络安全/权威随机事件。
    uint32 Value = static_cast<uint32>(RandomSeed) ^
        (static_cast<uint32>(BirdIndex + 1) * 0x9E3779B9u) ^ Salt;
    Value ^= Value >> 16;
    Value *= 0x7FEB352Du;
    Value ^= Value >> 15;
    Value *= 0x846CA68Bu;
    Value ^= Value >> 16;
    return static_cast<float>(Value & 0x00FFFFFFu) / 16777216.0f;
}
