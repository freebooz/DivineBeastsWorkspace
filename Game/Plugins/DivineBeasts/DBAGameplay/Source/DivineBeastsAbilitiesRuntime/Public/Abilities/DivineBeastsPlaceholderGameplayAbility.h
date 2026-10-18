#pragma once

#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Types/GamePlatformCombatSpec.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "TimerManager.h"
#include "DivineBeastsPlaceholderGameplayAbility.generated.h"

/** EDivineBeastsPlaceholderAbilityMode（临时技能执行类型），后期由正式技能类取代。 */
UENUM(BlueprintType)
enum class EDivineBeastsPlaceholderAbilityMode : uint8
{
    Primary,       // 普通攻击：权威前向单体伤害
    Passive,       // 被动：暂不实现，禁止主动释放
    Fireball,      // 火焰弹：单枚可选目标追踪弹体
    Frostbolt,     // 寒冰箭：单枚可选目标追踪弹体
    ArcaneMissiles, // 奥术飞弹：三枚螺旋连射弹体
    Healing        // 治疗：枚举值5，保留Primary及历史资产枚举值不迁号；输入由授权集决定。
};

/**
 * 临时生肖技能执行器：只使用项目统一Definition、GAS成本/冷却及平台Combat/Projectile权威结果。
 * 这是获得用户明确批准的可替换占位技能，不依赖开发技能开关，不冒充十二生肖最终技能设计。
 * 行为类型由各英雄自己的蓝图CDO配置；各英雄独立AbilityId，不在这里推导生肖数值。
 * 无Niagara或UI依赖；客户端只显示服务器复制的Cast和Projectile事实。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsPlaceholderGameplayAbility
    : public UDivineBeastsConfiguredGameplayAbility
{
    GENERATED_BODY()

public:
    UDivineBeastsPlaceholderGameplayAbility();

    /** 普攻/被动/三类法术/治疗的行为；Primary资产保持稳定身份并显式迁移到Healing。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    EDivineBeastsPlaceholderAbilityMode PlaceholderMode =
        EDivineBeastsPlaceholderAbilityMode::Passive;

    /** 客户端仅用于展示，投射物实际运动在服务器生成。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FGamePlatformCombatProjectileMotion ProjectileMotion;

    /** 项目纯内容表现语义覆盖；空值使用行为默认值，不改变弹道、伤害或服务器资格。
     * 客户端仍须通过精确语义/能力身份允许列表，不允许据此加载任意资源路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FName PresentationStyleOverride = NAME_None;

    /** 单次激活生成数量，火球/冰箭为1，奥术为3。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder",
        meta=(ClampMin="1", ClampMax="3"))
    int32 ProjectileCount = 1;

    /** 连射间隔，单发为0；不放宽平台投射物结算规则。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder",
        meta=(ClampMin="0", Units="s"))
    float ProjectileIntervalSeconds = 0.0f;

    /** 使用既有能力GAS授予的冷却标签，禁止自行在客户端计时。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FGameplayTagContainer PlaceholderCooldownTags;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    /** 三类占位法术及治疗使用原生GAS无成本合同，抵御旧蓝图成本引用；普通攻击继续遵循原配置。 */
    virtual UGameplayEffect* GetCostGameplayEffect() const override;

    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual const FGameplayTagContainer* GetCooldownTags() const override;

private:
    /** 本次激活的所有权代次，取消、死亡和重新激活时拒绝迟到弹体。 */
    FGuid ActivationToken;
    /** 激活拥有来源强引用；由GC跟踪，终态统一释放。 */
    UPROPERTY(Transient) FGamePlatformCombatSpec CastSpec;
    /** 跨连发仅保存弱身份；最初无目标与原锁定目标销毁必须区分，失效不能降级直线。 */
    TWeakObjectPtr<AActor> FrozenProjectileTarget;
    bool bHadFrozenTarget = false;
    FName CastStyle = NAME_None;
    int32 RemainingShots = 0;
    int32 ReleasedShots = 0;
    FTimerHandle BurstTimer;

    void ReleaseNextProjectile();
};
