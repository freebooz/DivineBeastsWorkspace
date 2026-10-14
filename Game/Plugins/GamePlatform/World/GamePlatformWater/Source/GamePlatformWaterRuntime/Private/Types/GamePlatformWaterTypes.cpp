// 纯CPU权威水域采样：不引用GPU深度或波浪材质，服务器和客户端输入一致。
// 静态区域是水域玩法的一期依据，未来动态水量变化需要有单独复制契约。
#include "Types/GamePlatformWaterTypes.h"

bool FGamePlatformWaterMath::SampleWaterVolume(
    const FTransform& BoxWorldTransform, const FVector& UnscaledBoxHalfExtent,
    float SurfaceZCm, const FVector& WorldPosition,
    const FGamePlatformWaterRegionConfig& Config, FGamePlatformWaterSample& OutSample)
{
    OutSample = FGamePlatformWaterSample();
    if (!Config.IsValid() || !FMath::IsFinite(SurfaceZCm) ||
        !FMath::IsFinite(WorldPosition.X) || !FMath::IsFinite(WorldPosition.Y) ||
        !FMath::IsFinite(WorldPosition.Z) || !FMath::IsFinite(UnscaledBoxHalfExtent.X) ||
        !FMath::IsFinite(UnscaledBoxHalfExtent.Y) || !FMath::IsFinite(UnscaledBoxHalfExtent.Z) ||
        UnscaledBoxHalfExtent.GetMin() <= 0.f)
    {
        return false;
    }

    const FVector Local = BoxWorldTransform.InverseTransformPosition(WorldPosition);
    // Local为含旋转与缩放的Box局部厘米坐标；不使用世界包围盒近似横向范围。
    if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y) ||
        !FMath::IsFinite(Local.Z) ||
        FMath::Abs(Local.X) > UnscaledBoxHalfExtent.X ||
        FMath::Abs(Local.Y) > UnscaledBoxHalfExtent.Y ||
        FMath::Abs(Local.Z) > UnscaledBoxHalfExtent.Z ||
        WorldPosition.Z > SurfaceZCm)
    {
        return false;
    }

    OutSample.bInWater = true;
    OutSample.RegionId = Config.RegionId;
    OutSample.Kind = Config.Kind;
    OutSample.SurfaceZCm = SurfaceZCm;
    OutSample.DepthCm = FMath::Max(0.f, SurfaceZCm - static_cast<float>(WorldPosition.Z));
    OutSample.bShallow = OutSample.DepthCm <= Config.ShallowDepthCm;
    OutSample.bSwimmable = Config.bSwimmable;
    OutSample.bAllowLocalRipples = Config.bAllowLocalRipples;
    const FVector2D Direction = Config.FlowDirection.GetSafeNormal();
    OutSample.FlowVelocityCmPerSecond = FVector(
        Direction.X * Config.FlowSpeedCmPerSecond,
        Direction.Y * Config.FlowSpeedCmPerSecond, 0.0);
    return true;
}
