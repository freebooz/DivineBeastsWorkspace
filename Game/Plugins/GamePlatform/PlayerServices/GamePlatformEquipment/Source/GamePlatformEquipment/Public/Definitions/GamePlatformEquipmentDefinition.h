#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GamePlatformEquipmentDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMEQUIPMENT_API UGamePlatformEquipmentDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName CompatibleItemDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> AllowedSlotIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> AbilitySetDefinitionIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> GameplayEffectDefinitionIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FGameplayTagContainer GameplayTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin="1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName RequirementId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName RequiredProgressionTrackId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin="0"))
    int32 RequiredLevel = 0;

    bool IsStructurallyValid() const
    {
        return !EquipmentDefinitionId.IsNone() &&
               !CompatibleItemDefinitionId.IsNone() &&
               !AllowedSlotIds.IsEmpty() &&
               Version > 0;
    }

    bool SupportsSlot(FName SlotId) const
    {
        return !SlotId.IsNone() && AllowedSlotIds.Contains(SlotId);
    }
};
