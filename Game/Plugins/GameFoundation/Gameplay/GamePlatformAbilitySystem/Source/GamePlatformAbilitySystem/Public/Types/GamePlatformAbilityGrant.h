#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformAbilityGrant.generated.h"
class UGamePlatformGameplayAbility;
class UGameplayEffect;
class UGamePlatformAttributeSet;

/** 同一ASC中同逻辑ID、同技能类或同输入标签重复均拒绝，不按加载顺序覆盖。 */
UENUM(BlueprintType)
enum class EGamePlatformAbilityGrantPolicy : uint8 { RejectDuplicates };
/** 启动效果必须可撤销；瞬时效果由技能提交阶段产生，不支持在可回滚授权事务中应用。 */
UENUM(BlueprintType)
enum class EGamePlatformEffectGrantPolicy : uint8 { WhileGranted };
/** 已创建属性集保留到ASC关闭，避免动态移除后迟到属性复制或外部效果悬空。 */
UENUM(BlueprintType)
enum class EGamePlatformAttributeLifetime : uint8 { UntilASCShutdown };

/** 一项服务器技能授权；所有软类必须通过AbilitySet租约的AbilitySet分组加载。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityGrant
{
    GENERATED_BODY()
    /** 稳定技能身份；不是类路径、输入标签或授权实例ID。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") FGamePlatformId AbilityId;
    /** 仅平台技能子类，第一版必须InstancedPerActor且可取消。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet", meta=(AssetBundles="AbilitySet")) TSoftClassPtr<UGamePlatformGameplayAbility> AbilityClass;
    /** 整数等级1..100，不来自客户端请求。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet", meta=(ClampMin="1", ClampMax="100")) int32 AbilityLevel = 1;
    /** 可空；非空须为Platform.Ability.Input的子标签，精确匹配到一个Spec。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") FGameplayTag InputTag;
    /** 可选中立来源标签，只作Spec来源元数据，不代表认证。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") FGameplayTag SourceTag;
    /** 当前唯一确定策略为拒绝重复。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") EGamePlatformAbilityGrantPolicy GrantPolicy = EGamePlatformAbilityGrantPolicy::RejectDuplicates;
};

/** 启动时可回滚的效果条目；定义不可变，每次应用创建独立Spec。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformEffectGrant
{
    GENERATED_BODY()
    /** 集合内唯一逻辑身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") FGamePlatformId EffectId;
    /** 有限持续或无限非堆叠效果，禁止瞬时／周期／执行计算等不可回滚启动副作用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet", meta=(AssetBundles="AbilitySet")) TSoftClassPtr<UGameplayEffect> EffectClass;
    /** 效果等级，有限值1..100。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") float Level = 1.f;
    /** 随本授权持有，撤销只移除本次真实返回的句柄；自然到期后不再次扣除。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") EGamePlatformEffectGrantPolicy ApplicationPolicy = EGamePlatformEffectGrantPolicy::WhileGranted;
};

/** 属性集条目；以类CDO默认值初始化一次，后续使用GAS效果修改，不复制第二套数值框架。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAttributeGrant
{
    GENERATED_BODY()
    /** 属性类。同ASC同类可复用本组件已创建实例，拒绝接管外部已创建实例。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet", meta=(AssetBundles="AbilitySet")) TSoftClassPtr<UGamePlatformAttributeSet> AttributeSetClass;
    /** 不支持运行中移除属性集；相应类资源租约保留至组件关闭。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilitySet") EGamePlatformAttributeLifetime Lifetime = EGamePlatformAttributeLifetime::UntilASCShutdown;
};
