// 鸟群高级配置校验与幂等判定；只是客户端语义，不创建Niagara或新资源租约。
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"

bool FDivineBeastsBirdFlockFlightSettings::ValidateForWorld(const UWorld* World, FString& OutReason) const
{
    OutReason.Reset();
    const int32 Mode = static_cast<int32>(FlightMode);
    if (Mode < 0 || Mode > 5 || Seed < 0 || Seed > 1000000 ||
        !FMath::IsFinite(QualityFraction) || QualityFraction < 0.f || QualityFraction > 1.f ||
        !FMath::IsFinite(GlideBlend) || GlideBlend < 0.f || GlideBlend > 1.f ||
        !FMath::IsFinite(SeparationWeight) || !FMath::IsFinite(AlignmentWeight) ||
        !FMath::IsFinite(CohesionWeight))
    {
        OutReason = TEXT("鸟群飞行模式、画质、滑翔、群体权重或随机种子越界。");
        return false;
    }
    if (bEnableFlocking &&
        (SeparationWeight < 0.f || AlignmentWeight < 0.f || CohesionWeight < 0.f ||
         SeparationWeight > 4.f || AlignmentWeight > 4.f || CohesionWeight > 4.f ||
         SeparationWeight + AlignmentWeight + CohesionWeight > 4.f))
    {
        OutReason = TEXT("Boids三项权重需0..4且总和不超过4。");
        return false;
    }
    if (bUseSpline != (Mode >= 3))
    {
        OutReason = TEXT("样条开关和飞行模式不匹配。");
        return false;
    }
    if (bUseSpline)
    {
        const USplineComponent* Source = Spline.Get();
        if (!World || !IsValid(Source) || Source->GetWorld() != World || !Source->IsRegistered() ||
            !FMath::IsFinite(Source->GetSplineLength()) || Source->GetSplineLength() <= 1.f ||
            !IsValid(Source->GetOwner()) || Source->GetOwner()->IsActorBeingDestroyed())
        {
            OutReason = TEXT("Spline来源未就绪、属于其它世界、已销毁或路径长度无效。");
            return false;
        }
    }
    return true;
}

bool FDivineBeastsBirdFlockFlightSettings::EquivalentTo(const FDivineBeastsBirdFlockFlightSettings& Other) const
{
    return FlightMode == Other.FlightMode && bUseSpline == Other.bUseSpline &&
        bEnableFlocking == Other.bEnableFlocking &&
        FMath::IsNearlyEqual(SeparationWeight, Other.SeparationWeight) &&
        FMath::IsNearlyEqual(AlignmentWeight, Other.AlignmentWeight) &&
        FMath::IsNearlyEqual(CohesionWeight, Other.CohesionWeight) &&
        FMath::IsNearlyEqual(GlideBlend, Other.GlideBlend) &&
        FMath::IsNearlyEqual(QualityFraction, Other.QualityFraction) &&
        Seed == Other.Seed && Spline == Other.Spline;
}
