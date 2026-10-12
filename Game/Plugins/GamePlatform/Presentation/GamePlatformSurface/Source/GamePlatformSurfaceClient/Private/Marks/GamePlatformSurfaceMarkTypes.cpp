// 纯值检查：不允许NaN、无效法线或无界投影尺寸进入渲染线程。
#include "Marks/GamePlatformSurfaceMarkTypes.h"

bool FGamePlatformSurfaceMarkPolicy::IsValidKind(EGamePlatformSurfaceMarkKind Kind)
{
    return Kind >= EGamePlatformSurfaceMarkKind::Bullet && Kind <= EGamePlatformSurfaceMarkKind::Crack;
}

bool FGamePlatformSurfaceMarkPolicy::IsValidRequest(const FGamePlatformSurfaceMarkRequest& Request)
{
    return IsValidKind(Request.Kind)
        && !Request.Location.ContainsNaN()
        && !Request.Normal.ContainsNaN()
        && !Request.Direction.ContainsNaN()
        && Request.Normal.SizeSquared() > UE_SMALL_NUMBER
        && FMath::IsFinite(Request.SizeScale)
        && Request.SizeScale >= 0.1f
        && Request.SizeScale <= 8.0f;
}
