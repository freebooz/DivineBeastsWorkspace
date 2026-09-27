#include "HitValidation/GamePlatformCombatHitValidator.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Settings/GamePlatformCombatSettings.h"

namespace
{
EGamePlatformCombatError ValidateCommon(
    AActor* Source,
    AActor* ExpectedTarget,
    const FVector& Start,
    float RequestedDistance)
{
    if (!IsValid(Source))
    {
        return EGamePlatformCombatError::InvalidSource;
    }

    if (!Source->HasAuthority())
    {
        return EGamePlatformCombatError::NotAuthority;
    }

    if (!IsValid(ExpectedTarget) ||
        ExpectedTarget->GetWorld() != Source->GetWorld())
    {
        return EGamePlatformCombatError::InvalidTarget;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();

    if (!FMath::IsFinite(RequestedDistance) ||
        RequestedDistance < 0.0f ||
        RequestedDistance > Settings->MaxHitDistance)
    {
        return EGamePlatformCombatError::OutOfRange;
    }

    if (FVector::DistSquared(Start, Source->GetActorLocation()) >
        FMath::Square(Settings->MaxTraceOriginOffset))
    {
        return EGamePlatformCombatError::HitValidationFailed;
    }

    return EGamePlatformCombatError::None;
}

void FillHit(
    const FVector& Start,
    const FVector& End,
    const FHitResult& Hit,
    FGamePlatformCombatHitContext& OutHit)
{
    OutHit.bHasValidatedHit = true;
    OutHit.TraceStart = Start;
    OutHit.TraceEnd = End;
    OutHit.ImpactPoint = Hit.ImpactPoint;
    OutHit.ImpactNormal = Hit.ImpactNormal;
    OutHit.ValidatedDistance = FVector::Distance(Start, Hit.ImpactPoint);
}
}

EGamePlatformCombatError FGamePlatformCombatHitValidator::ValidateServerLineTrace(
    AActor* Source,
    AActor* ExpectedTarget,
    const FVector& TraceStart,
    const FVector& Direction,
    float MaxDistance,
    ECollisionChannel CollisionChannel,
    FGamePlatformCombatHitContext& OutHit)
{
    OutHit = {};

    const EGamePlatformCombatError Common =
        ValidateCommon(Source, ExpectedTarget, TraceStart, MaxDistance);
    if (Common != EGamePlatformCombatError::None)
    {
        return Common;
    }

    const FVector SafeDirection = Direction.GetSafeNormal();
    if (SafeDirection.IsNearlyZero())
    {
        return EGamePlatformCombatError::HitValidationFailed;
    }

    const FVector TraceEnd = TraceStart + SafeDirection * MaxDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GamePlatformCombatLineTrace), false);
    Params.AddIgnoredActor(Source);

    FHitResult Hit;
    const bool bHit = Source->GetWorld()->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        CollisionChannel,
        Params);

    if (!bHit)
    {
        return EGamePlatformCombatError::HitValidationFailed;
    }

    if (Hit.GetActor() != ExpectedTarget)
    {
        return EGamePlatformCombatError::Obstructed;
    }

    FillHit(TraceStart, TraceEnd, Hit, OutHit);
    return EGamePlatformCombatError::None;
}

EGamePlatformCombatError FGamePlatformCombatHitValidator::ValidateServerSphereSweep(
    AActor* Source,
    AActor* ExpectedTarget,
    const FVector& SweepStart,
    const FVector& SweepEnd,
    float Radius,
    ECollisionChannel CollisionChannel,
    FGamePlatformCombatHitContext& OutHit)
{
    OutHit = {};

    const float Distance = FVector::Distance(SweepStart, SweepEnd);
    const EGamePlatformCombatError Common =
        ValidateCommon(Source, ExpectedTarget, SweepStart, Distance);
    if (Common != EGamePlatformCombatError::None)
    {
        return Common;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();

    if (!FMath::IsFinite(Radius) ||
        Radius <= 0.0f ||
        Radius > Settings->MaxSweepRadius)
    {
        return EGamePlatformCombatError::HitValidationFailed;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(GamePlatformCombatSphereSweep), false);
    Params.AddIgnoredActor(Source);

    TArray<FHitResult> Hits;
    const bool bAnyHit = Source->GetWorld()->SweepMultiByChannel(
        Hits,
        SweepStart,
        SweepEnd,
        FQuat::Identity,
        CollisionChannel,
        FCollisionShape::MakeSphere(Radius),
        Params);

    if (!bAnyHit)
    {
        return EGamePlatformCombatError::HitValidationFailed;
    }

    for (const FHitResult& Hit : Hits)
    {
        if (Hit.GetActor() == ExpectedTarget)
        {
            FillHit(SweepStart, SweepEnd, Hit, OutHit);
            return EGamePlatformCombatError::None;
        }
    }

    return EGamePlatformCombatError::HitValidationFailed;
}
