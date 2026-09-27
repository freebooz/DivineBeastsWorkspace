#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformQuestTypes.h"
#include "GamePlatformQuestDefinition.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMQUEST_API FGamePlatformQuestObjectiveDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName ObjectiveId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestObjectiveType ObjectiveType =
        EGamePlatformQuestObjectiveType::Tutorial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin="0.000001"))
    double RequiredValue = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName EventType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FGameplayTagContainer RequiredSemanticTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName RequiredRegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestAggregationPolicy AggregationPolicy =
        EGamePlatformQuestAggregationPolicy::Count;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    bool bOptional = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMQUEST_API FGamePlatformQuestPrerequisite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName CompletedQuestId = NAME_None;
};

UCLASS(BlueprintType)
class GAMEPLATFORMQUEST_API UGamePlatformQuestDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName QuestId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin="1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FGameplayTagContainer QuestTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    TArray<FGamePlatformQuestObjectiveDefinition> Objectives;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    TArray<FGamePlatformQuestPrerequisite> Prerequisites;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestRepeatPolicy RepeatPolicy =
        EGamePlatformQuestRepeatPolicy::OneShot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    EGamePlatformQuestTimeWindowPolicy TimeWindowPolicy =
        EGamePlatformQuestTimeWindowPolicy::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName RewardSetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    bool bCanAbandon = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    TArray<FName> RequiredDefinitions;

    bool ValidateDefinition(FText& OutReason) const;
};
