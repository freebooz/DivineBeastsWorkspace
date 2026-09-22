#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformAbilityGrant.h"
#include "GamePlatformAbilitySetDefinition.generated.h"

/** 只读能力集；继承LogicalId、DataVersion和依赖闭包，不产生新的ID或资源管理器。 */
UCLASS(BlueprintType)
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformAbilitySetDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 至少一项技能、效果或属性，单类条目最多64；授予顺序属性→效果→技能。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") TArray<FGamePlatformAbilityGrant> Abilities;
    /** 全部启动效果在该授权移除时独立撤销。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") TArray<FGamePlatformEffectGrant> Effects;
    /** CDO初始化且生命周期到ASC关闭，不承诺跨重生保留。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") TArray<FGamePlatformAttributeGrant> Attributes;
    /** 默认非开发；开发集合还需显式FoundationAbilitySystem开关且非Shipping，资产Cook另行隔离。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") bool bDevelopmentOnly = false;
    /** 纯字段校验，不同步加载；加载后ASC还会校验真实类型、网络策略和效果可回滚性。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
