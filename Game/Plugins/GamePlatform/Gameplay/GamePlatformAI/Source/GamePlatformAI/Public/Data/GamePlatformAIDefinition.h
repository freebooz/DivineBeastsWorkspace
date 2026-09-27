#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformAITypes.h"
#include "GamePlatformAIDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMAI_API UGamePlatformAIDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FName AIDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    EGamePlatformAIBrainType BrainType =
        EGamePlatformAIBrainType::BehaviorTree;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Brain")
    FSoftObjectPath BehaviorTreeAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Brain")
    FSoftObjectPath BlackboardAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Brain")
    FSoftObjectPath StateTreeAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FGamePlatformAIPerceptionProfile PerceptionProfile;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FGamePlatformAITargetSelectionProfile TargetSelectionProfile;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FGamePlatformAIHomePolicy HomePolicy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FGamePlatformAIUpdateProfile UpdateProfile;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Navigation")
    FName NavigationProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Combat")
    FName CombatProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Combat")
    FGameplayTag PrimaryAbilityTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Combat", meta=(ClampMin="0.0"))
    float AttackRange = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Combat", meta=(ClampMin="0.0"))
    float PreferredRange = 140.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Dependencies")
    TArray<FName> RequiredDefinitions;

    bool ValidateDefinition(FText& OutReason) const;
    void GetReferencedAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
