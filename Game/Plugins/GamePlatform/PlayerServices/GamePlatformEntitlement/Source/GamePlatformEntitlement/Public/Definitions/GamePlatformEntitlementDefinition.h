#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "GamePlatformEntitlementDefinition.generated.h"

/**
 * 平台Entitlement Definition（权益定义）。
 * 只保存稳定ID和中立目标类型；具体Hero/Skin目录由项目层映射。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMENTITLEMENT_API UGamePlatformEntitlementDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 稳定权益定义身份；None无效，不等于具体项目资源路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;

    /** 中立权益分类身份；None无效，由项目映射具体展示。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName Category = NAME_None;

    /** 中立权益目标类型；Unknown无效，不绑定特定游戏英雄。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementTargetType TargetType =
        EGamePlatformEntitlementTargetType::Unknown;

    /** 目标定义稳定身份；None无效，授权真源仍在后端。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName TargetId = NAME_None;

    /** 定义正整数版本，默认1；稳定身份/迁移由数据与发布合同处理。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement", meta=(ClampMin="1"))
    int32 Version = 1;

    /** 可选中立展示元数据身份；None无展示，失败不改变权益。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName DisplayMetadataId = NAME_None;

    /** 默认展示策略语义，默认Locked；不代表后端已授予/撤销权益。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName DefaultPolicy = TEXT("Locked");

    /** 仅检查定义必需身份/版本/范围；不加载资源或验证后端授权，false表示不可登记。 */
    bool IsStructurallyValid() const
    {
        return !EntitlementId.IsNone() &&
               !Category.IsNone() &&
               TargetType != EGamePlatformEntitlementTargetType::Unknown &&
               !TargetId.IsNone() &&
               Version > 0;
    }
};
