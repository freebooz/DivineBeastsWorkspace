#pragma once

#include "CoreMinimal.h"
#include "GamePlatformCombatHitContext.generated.h"

/** 服务器命中事实；客户端提供的 HitResult 不能直接成为权威结果。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatHitContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    bool bHasValidatedHit = false;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector TraceStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector TraceEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector ImpactNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float ValidatedDistance = 0.0f;
};
