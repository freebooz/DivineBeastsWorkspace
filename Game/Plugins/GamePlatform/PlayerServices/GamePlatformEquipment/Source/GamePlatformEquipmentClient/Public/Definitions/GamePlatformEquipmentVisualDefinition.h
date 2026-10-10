// 平台中立静态定义；由数据服务/内容拥有，调用方先加载验证；客户端视觉软引用失败不改变服务器权威。
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
    /** 客户端外观定义唯一身份；None无效，不改变装备规则。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FName EquipmentVisualId = NAME_None;

    /** 客户端视觉执行方式；当前仅支持StaticMesh，不是权威碰撞。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    EGamePlatformEquipmentVisualType VisualType =
        EGamePlatformEquipmentVisualType::StaticMesh;

    /** 可选静态网格软引用；缺失/加载失败只清理视觉，不改变装备权威。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    /** 挂接Avatar插槽身份；None使用默认挂接，命名插槽缺失报告视觉错误。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FName SocketName = NAME_None;

    /** 相对于挂接插槽的变换，采用UE单位（位置厘米、旋转度、无单位缩放）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FTransform RelativeTransform;

    /** 按材质槽顺序应用的可选材质软引用；加载句柄归视觉组件本次请求。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    /** 客户端表现语义标签；不参与伤害/权限/库存计算。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EquipmentVisual")
    FGameplayTagContainer PresentationTags;
};
