#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformInventoryItemDefinition.generated.h"

/**
 * 客户端ItemDefinition（物品显示定义）。
 * 仅用于显示和本地分类；PlayerData（玩家数据服务）独立执行长期背包权威规则，
 * 绝不能信任客户端资产中的 MaxStackSize、标签或显示字段作为服务器规则来源。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMINVENTORYCLIENT_API UGamePlatformInventoryItemDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 使用稳定 ItemDefinitionId 形成 PrimaryAssetId（主资产编号），便于项目层通过 Asset Manager
     * 异步解析显示资产；无效定义退回 UPrimaryDataAsset 默认身份，避免注册 NAME_None。
     */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        static const FPrimaryAssetType InventoryItemType(
            TEXT("GamePlatformInventoryItemDefinition"));

        return IsStructurallyValid()
            ? FPrimaryAssetId(InventoryItemType, ItemDefinitionId)
            : Super::GetPrimaryAssetId();
    }

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
