#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformCombatHitContext.h"
#include "Types/GamePlatformCombatTypes.h"
#include "GamePlatformCombatSpec.generated.h"

class AActor;

/** 一次权威战斗结算的中立规格。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    FGuid EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    TObjectPtr<AActor> Source = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    float Magnitude = 0.0f;

    /** 伤害类型；治疗路径忽略该字段。默认Untyped保持历史伤害兼容。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Damage")
    EGamePlatformDamageType DamageType = EGamePlatformDamageType::Untyped;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    FGameplayTagContainer CombatTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    FGameplayEffectContextHandle EffectContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    FGamePlatformCombatHitContext HitContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    FName SourceAbilityId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    int32 SourceAvatarGeneration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    int32 TargetAvatarGeneration = 0;
};
