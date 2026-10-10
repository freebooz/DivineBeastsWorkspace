// 平台中立静态定义；由数据服务/内容拥有，调用方先加载验证；客户端视觉软引用失败不改变服务器权威。
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
    /** 平台装备规则定义身份；None无效，资源由数据服务/调用方拥有。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    /** 可装备的物品定义身份；服务器必须与实例ItemDefinitionId一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName CompatibleItemDefinitionId = NAME_None;

    /** 该定义支持的非空槽位集合；不是服务器已装备状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> AllowedSlotIds;

    /** 需授予的能力集合定义身份；服务器解析器必须解析成功才授予。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> AbilitySetDefinitionIds;

    /** 需施加的GameplayEffect定义身份；每次授予句柄负责撤销自己的效果。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    TArray<FName> GameplayEffectDefinitionIds;

    /** 装备规则语义标签；不替代服务器权限与库存校验。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FGameplayTagContainer GameplayTags;

    /** 定义正整数版本；默认1，稳定身份迁移由数据/内容合同处理。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin="1"))
    int32 Version = 1;

    /** 可选外观定义身份；None表示无装备视觉，不影响服务器权威装备。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;

    /** 可选服务器装备资格规则身份；None表示未声明，当前组件不执行额外规则服务。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName RequirementId = NAME_None;

    /** 可选前置成长轨道身份；None未声明，权威资格仍由服务器策略验证。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
    FName RequiredProgressionTrackId = NAME_None;

    /** 非负整数前置等级；0未声明，客户端显示不能替代后端验证。 */
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
