#pragma once

#include "Abilities/GameplayAbility.h"
#include "Types/GamePlatformAbilityInput.h"
#include "GamePlatformGameplayAbility.generated.h"

/**
 * UGamePlatformGameplayAbility（游戏平台技能基类）。
 *
 * 该类型是 GamePlatformAbilitySystem（平台技能系统）公开 AbilitySet 契约要求的稳定基类，
 * 同时声明 InputTag（输入标签）对应能力在按下或持续按住时的通用激活策略；
 * 不包含具体游戏、生肖、MOBA、实体按键或项目表现逻辑。
 *
 * 第一阶段默认采用 InstancedPerActor（每Actor实例化），与现有 AbilitySet 授权事务的
 * “可取消、可按真实 SpecHandle 撤销”合同保持一致。具体技能仍可在派生类中声明标签、
 * 成本、冷却和执行逻辑，但不得绕过服务器权威与项目准入边界。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformGameplayAbility
    : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGamePlatformGameplayAbility();
    /**
     * 平台能力所有GAS激活入口统一经过当前Avatar的项目资格Gate，再执行原生费用/冷却/标签检查。
     * 仅游戏线程同步读取；无Gate拒绝。原生派生不能跳过此门禁，额外资格使用GAS标签或K2事件。
     */
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override final;

    /**
     * 输入触发策略。
     * OnPressed（按下触发）只在一次新按压时尝试激活；WhileHeld（按住触发）允许输入保持期间继续尝试。
     * 输入适配仅读取能力默认对象上的配置，不在高频输入路径创建对象或加载资产。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Ability|Input")
    EGamePlatformAbilityActivationPolicy ActivationPolicy =
        EGamePlatformAbilityActivationPolicy::OnPressed;
};
