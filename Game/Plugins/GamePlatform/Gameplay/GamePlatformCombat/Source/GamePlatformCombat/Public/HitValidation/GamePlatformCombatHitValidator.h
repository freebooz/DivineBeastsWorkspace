#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformCombatHitContext.h"
#include "Types/GamePlatformCombatTypes.h"

class AActor;

/** 服务器当前时刻世界状态命中验证；不实现Server Rewind。 */
class GAMEPLATFORMCOMBAT_API FGamePlatformCombatHitValidator
{
public:
    static EGamePlatformCombatError ValidateServerLineTrace(
        AActor* Source,
        AActor* ExpectedTarget,
        const FVector& TraceStart,
        const FVector& Direction,
        float MaxDistance,
        ECollisionChannel CollisionChannel,
        FGamePlatformCombatHitContext& OutHit);

    static EGamePlatformCombatError ValidateServerSphereSweep(
        AActor* Source,
        AActor* ExpectedTarget,
        const FVector& SweepStart,
        const FVector& SweepEnd,
        float Radius,
        ECollisionChannel CollisionChannel,
        FGamePlatformCombatHitContext& OutHit);
};
