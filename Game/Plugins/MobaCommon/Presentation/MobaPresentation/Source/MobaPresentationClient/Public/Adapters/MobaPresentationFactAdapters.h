#pragma once

#include "CoreMinimal.h"
#include "Types/MobaPresentationTypes.h"

struct FGamePlatformCombatEvent;

/**
 * FMobaPresentationFactAdapters（MOBA表现事实适配器集合）。
 * 只把上游可信/已复制事实转换为语义；绝不修改Arena/Combat/Ability权威状态。
 */
class MOBAPRESENTATIONCLIENT_API FMobaPresentationFactAdapters
{
public:
    static void FromCombatEvent(
        const FGamePlatformCombatEvent& Event,
        TArray<FMobaPresentationAdaptedFact>& OutFacts);

    static FMobaPresentationAdaptedFact FromAbilityFact(
        const FMobaPresentationAbilityFact& Fact);

    static FMobaPresentationAdaptedFact FromStatusFact(
        const FMobaPresentationStatusFact& Fact);

    static FMobaPresentationAdaptedFact FromCharacterFact(
        const FMobaPresentationCharacterFact& Fact);

    static FMobaPresentationAdaptedFact FromArenaFact(
        const FMobaPresentationArenaFact& Fact);

    static FGuid MakeRevisionFactId(
        const FString& Scope,
        int32 Revision,
        uint32 Salt = 0);
};
