#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** FMobaPresentationSemanticDefinition（MOBA表现语义静态定义）。 */
struct MOBAPRESENTATIONRUNTIME_API FMobaPresentationSemanticDefinition
{
    FGameplayTag Tag;
    FName OwnerModule = TEXT("MobaPresentationRuntime");
    FString SemanticMeaning;
    FString SourceFact;
    bool bTransient = true;
    bool bDeprecated = false;
    FGameplayTag Replacement;
};

/** FMobaPresentationSemanticRegistry（MOBA表现语义静态注册表）。 */
class MOBAPRESENTATIONRUNTIME_API FMobaPresentationSemanticRegistry
{
public:
    static const TArray<FMobaPresentationSemanticDefinition>& GetAll();
    static const FMobaPresentationSemanticDefinition* Find(const FGameplayTag& Tag);
    static bool Validate(TArray<FString>& OutErrors);
    static bool IsTransient(const FGameplayTag& Tag);
};
