#include "Marks/GamePlatformSurfaceMarkCatalog.h"

bool FGamePlatformSurfaceMarkStyle::IsValidStyle() const
{
    return FGamePlatformSurfaceMarkPolicy::IsValidKind(Kind)
        && Material.ToSoftObjectPath().IsValid()
        && !SizeCm.ContainsNaN()
        && SizeCm.X > 0.01 && SizeCm.Y > 0.01 && SizeCm.Z > 0.01
        && FMath::IsFinite(LifetimeSeconds) && LifetimeSeconds >= 0.0f
        && FMath::IsFinite(FadeSeconds) && FadeSeconds >= 0.0f
        && FMath::IsFinite(NormalOffsetCm) && FMath::Abs(NormalOffsetCm) <= 50.0f;
}

const FGamePlatformSurfaceMarkStyle* UGamePlatformSurfaceMarkCatalog::FindStyle(
    EGamePlatformSurfaceMarkKind Kind, EPhysicalSurface SurfaceType) const
{
    const FGamePlatformSurfaceMarkStyle* Fallback = nullptr;
    for (const auto& Style : Styles)
    {
        if (Style.Kind != Kind) continue;
        if (Style.SurfaceType == SurfaceType) return &Style;
        if (Style.SurfaceType == SurfaceType_Default && !Fallback) Fallback = &Style;
    }
    return Fallback;
}

bool UGamePlatformSurfaceMarkCatalog::IsValidCatalog(FString& OutError) const
{
    if (MaxActiveMarks < 1 || MaxActiveMarks > 512 ||
        MaxPendingMarks < 1 || MaxPendingMarks > 128 ||
        MaxRecentEventIds < 1 || MaxRecentEventIds > 2048)
    {
        OutError = TEXT("痕迹预算不在允许范围。");
        return false;
    }
    TSet<uint64> Seen;
    for (const auto& Style : Styles)
    {
        if (!Style.IsValidStyle())
        {
            OutError = TEXT("痕迹存在非法材质、尺寸或淡出参数。");
            return false;
        }
        const uint64 Key = (uint64(static_cast<uint8>(Style.Kind)) << 32) |
            uint32(static_cast<uint8>(Style.SurfaceType.GetValue()));
        if (Seen.Contains(Key))
        {
            OutError = TEXT("同类别与表面类型配置重复。");
            return false;
        }
        Seen.Add(Key);
    }
    for (uint8 Index = 0; Index < 5; ++Index)
    {
        if (!FindStyle(static_cast<EGamePlatformSurfaceMarkKind>(Index), SurfaceType_Default))
        {
            OutError = TEXT("缺少五种痕迹之一的默认材质回退。");
            return false;
        }
    }
    OutError.Reset();
    return true;
}
