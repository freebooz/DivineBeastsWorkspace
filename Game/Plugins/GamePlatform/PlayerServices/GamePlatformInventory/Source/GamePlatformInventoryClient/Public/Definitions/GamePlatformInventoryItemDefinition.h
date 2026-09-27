#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformInventoryItemDefinition.generated.h"

/**
 * 客户端ItemDefinition（物品显示定义）。
 * 仅用于显示和本地分类；Go PlayerData（玩家数据服务）独立验证MaxStack/Consumable等权威规则，
 * 绝不能信任客户端资产作为服务器规则来源。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMINVENTORYCLIENT_API UGamePlatformInventoryItemDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName DisplayNameKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName DescriptionKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName Category = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName IconId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
    int32 MaxStackSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    TArray<FName> ClientTags;

    bool IsStructurallyValid() const
    {
        return !ItemDefinitionId.IsNone() &&
               MaxStackSize > 0;
    }
};
