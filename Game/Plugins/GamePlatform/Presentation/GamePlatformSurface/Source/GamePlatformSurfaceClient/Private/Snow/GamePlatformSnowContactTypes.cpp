// 压雪纯值校验与每脚接地迟滞；不持有世界或材质，不推断服务器玩法。
#include "Types/GamePlatformSnowContactTypes.h"
bool FGamePlatformSnowContact::IsValid() const
{
    return EventId.IsValid() && !Location.ContainsNaN() && !Forward.ContainsNaN() &&
        Forward.SizeSquared2D() > UE_SMALL_NUMBER && FMath::IsFinite(LengthCm) &&
        FMath::IsFinite(WidthCm) && FMath::IsFinite(Pressure) &&
        LengthCm >= 5.f && LengthCm <= 80.f && WidthCm >= 3.f && WidthCm <= 50.f && Pressure > 0.f && Pressure <= 1.f;
}
bool FGamePlatformSnowFootContactGate::Update(const FVector& Foot, float Height, float Speed, bool bGrounded, double Now)
{
    if (Foot.ContainsNaN() || !FMath::IsFinite(Height) || !FMath::IsFinite(Speed) || !FMath::IsFinite(Now)) return false;
    if (!bGrounded || Height > 18.f) { bNearGround = false; return false; }
    if (Speed < 10.f || Height > 12.f || bNearGround) return false;
    bNearGround = true;
    if (bHasStamped && (Now-LastTime < .08 || FVector::DistSquared2D(Foot,LastLocation) < FMath::Square(12.f))) return false;
    bHasStamped = true; LastLocation = Foot; LastTime = Now;
    return true;
}
