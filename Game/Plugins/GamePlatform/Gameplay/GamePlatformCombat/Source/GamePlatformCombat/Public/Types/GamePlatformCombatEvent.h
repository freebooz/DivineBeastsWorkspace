#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformCombatTypes.h"
#include "GamePlatformCombatEvent.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FGuid EventId;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    EGamePlatformCombatEventType EventType = EGamePlatformCombatEventType::Damage;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    TObjectPtr<AActor> SourceActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    int32 SourceAvatarGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    int32 TargetAvatarGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float RequestedMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float AppliedMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float AppliedToShield = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    float AppliedToHealth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FGameplayTagContainer ResultTags;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FVector ImpactNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Combat")
    int32 WorldContextGeneration = 0;
};
