#pragma once

#include "Definitions/GamePlatformDefinitionBase.h"
#include "GameplayTagContainer.h"
#include "GamePlatformRegionDefinition.generated.h"

/** 当前区域边界能力；不等同于World Partition单元，也不提供尚未实现的形状。 */
UENUM(BlueprintType)
enum class EGamePlatformRegionBoundsPolicy : uint8
{
    /** 边界由本世界注册Provider提供轴对齐盒；定义资产本身不保存Actor实例或位置。 */
    AxisAlignedBox
};

/** 区域可参与查询的时机；只表达平台已实现能力，不代表流送完成或网络准入。 */
UENUM(BlueprintType)
enum class EGamePlatformRegionActivationPolicy : uint8
{
    /** Provider有效注册期间参与查询，撤销/销毁后退出；不强制该Provider永久驻留内存。 */
    AlwaysRegistered
};

/** 中立逻辑区域定义；RegionId就是继承的LogicalId，既不是Shard也不是服务器实例。
 * 游戏线程只读；资源需求沿用RequiredDefinitions，运行边界和观察主体由本世界Provider提供。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMWORLD_API UGamePlatformRegionDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 可选父区域逻辑身份；默认空值表示根区域，不能自指。
     * 父关系不自动形成资产租约；实际使用的父定义须由消费方纳入RequiredDefinitions，编辑器验证完整父链。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World|Region")
    FGamePlatformId ParentRegionId;

    /** 必填的中立区域语义名；不是FGameplayTag，不要求项目标签注册，不内置大厅/竞技规则。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World|Region")
    FName RegionTypeTag;

    /** 边界解释方式；第一版只支持轴对齐盒，非法枚举值明确失败而非回退。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World|Region")
    EGamePlatformRegionBoundsPolicy BoundsPolicy = EGamePlatformRegionBoundsPolicy::AxisAlignedBox;

    /** 注册期间的查询资格；第一版仅支持AlwaysRegistered，不自动激活流送或玩法。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World|Region")
    EGamePlatformRegionActivationPolicy ActivationPolicy = EGamePlatformRegionActivationPolicy::AlwaysRegistered;

    /** 可选中立Gameplay标签集合；标签注册沿用引擎机制，客户端标签不能授权服务器玩法结果。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World|Region")
    FGameplayTagContainer GameplayTags;

    /** 游戏线程无加载验证；先验证Data身份/版本/依赖，再验证父身份、语义与已实现策略。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
