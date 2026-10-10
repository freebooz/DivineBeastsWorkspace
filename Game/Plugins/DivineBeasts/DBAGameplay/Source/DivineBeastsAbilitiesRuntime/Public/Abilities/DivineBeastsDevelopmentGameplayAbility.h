#pragma once

#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "DivineBeastsDevelopmentGameplayAbility.generated.h"

/**
 * 项目双端开发验证能力：只在双重显式启用的非Shipping/Test进程执行服务器前向单目标伤害。
 * 继承既有配置技能的身份、预加载、可信命中和平台战斗结算；不实现图标所示位移、范围、治疗或被动。
 * 数据、GE与蓝图由开发目录拥有；本类不加载资产、不拥有另一个ASC，单次激活立即结束。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsDevelopmentGameplayAbility
    : public UDivineBeastsConfiguredGameplayAbility
{
    GENERATED_BODY()

public:
    /** 默认服务器独占，客户端只经现有ASC输入请求；不允许客户端本地提交伤害。 */
    UDivineBeastsDevelopmentGameplayAbility();

    /** 每个开发技能独立冷却标签；动态加入原生GE Spec，避免不同技能共用一条冷却。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Development")
    FGameplayTagContainer DevelopmentCooldownTags;

    /**
     * 作者依据候选正基础伤害和距离填写的客户端能力标记，默认false。
     * 客户端只需蓝图CDO即可灰显未实现槽位，不为高频可用性检查同步加载服务器逻辑定义；
     * 此标记不代表可信数值或命中，服务器激活仍核对实际定义、英雄身份、代次和正数伤害/范围。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Development")
    bool bDevelopmentDamageSampleSupported = false;

    /** 未启用或零伤害/零距离候选直接取消且不消费；成功Commit后真实射线并结束，未命中不退款。 */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    /** 返回稳定CDO配置供GAS原生CheckCooldown及装配数值门禁读取，不另建冷却计时器。 */
    virtual const FGameplayTagContainer* GetCooldownTags() const override;

    /**
     * 平台CanActivateAbility为final，使用GAS原生虚函数CheckCost执行开发候选可用性前置门禁。
     * 未启用及作者标记尚未实现的候选一律拒绝，界面仍保留真实输入槽并显示不可激活。
     * 通过资格后由Super校验真实资源；不改成本、不开辟第二套激活框架、不产生任何副作用。
     */
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    /** 创建正常GE Spec并授予同一组冷却标签；持续秒数由真实GE CDO定义和装配校验保证。 */
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
