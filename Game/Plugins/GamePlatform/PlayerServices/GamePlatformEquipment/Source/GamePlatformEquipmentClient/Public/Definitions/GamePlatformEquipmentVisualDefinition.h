#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GamePlatformEquipmentVisualDefinition.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EGamePlatformEquipmentVisualType : uint8
{
    StaticMesh
};

UCLASS(BlueprintType)
class GAMEPLATFORMEQUIPMENTCLIENT_API UGamePlatformEquipmentVisualDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FName EquipmentVisualId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    EGamePlatformEquipmentVisualType VisualType =
        EGamePlatformEquipmentVisualType::StaticMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FName SocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FTransform RelativeTransform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FGameplayTagContainer PresentationTags;
};
