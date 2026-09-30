#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformCombatTypes.h"
#include "GamePlatformCombatResult.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    EGamePlatformCombatError Error = EGamePlatformCombatError::None;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FGuid EventId;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float RequestedMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float FinalMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float AppliedToShield = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float AppliedToHealth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float RemainingShield = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float RemainingHealth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    bool bWasBlocked = false;

    /** 本次伤害是否命中平台通用暴击判定。 */
    UPROPERTY(BlueprintReadOnly, Category="Combat")
    bool bWasCritical = false;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    bool bCausedDeath = false;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FGameplayTagContainer ResultTags;

    bool IsSuccess() const { return Error == EGamePlatformCombatError::None; }
};
