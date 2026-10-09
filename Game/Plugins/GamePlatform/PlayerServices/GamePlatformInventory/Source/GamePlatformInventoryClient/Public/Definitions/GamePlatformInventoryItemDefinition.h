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

    /** 平台中立物品定义身份；None无效，不携带具体项目资产路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    /** 物品名称本地化键；None未提供，由客户端采用可读回退。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName DisplayNameKey = NAME_None;

    /** 产品说明本地化键；空值未提供，不作为机器错误码。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName DescriptionKey = NAME_None;

    /** 客户端物品分类语义；None未分类，不构成服务器规则。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName Category = NAME_None;

    /** 可选中立图标身份；None无图标，加载/映射归数据与表现服务。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    FName IconId = NAME_None;

    /** 客户端显示堆叠提示正整数，默认1；真实上限来自后端快照，不能据此消费/合并。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
    int32 MaxStackSize = 1;

    /** 仅客户端分类/展示标签集合；空集合合法，不向后端授予权限。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
    TArray<FName> ClientTags;

    /** 仅检查定义必需身份/版本/范围；不加载资源或验证后端授权，false表示不可登记。 */
    bool IsStructurallyValid() const
    {
        return !ItemDefinitionId.IsNone() &&
               MaxStackSize > 0;
    }
};
