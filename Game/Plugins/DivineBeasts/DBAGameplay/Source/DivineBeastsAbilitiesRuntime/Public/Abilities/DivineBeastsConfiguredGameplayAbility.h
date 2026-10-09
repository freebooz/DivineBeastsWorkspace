#pragma once

#include "CoreMinimal.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Types/GamePlatformCombatResult.h"
#include "Types/GamePlatformCombatHitContext.h"
#include "Types/DivineBeastsAbilityBalanceRow.h"
#include "UObject/PrimaryAssetId.h"
#include "DivineBeastsConfiguredGameplayAbility.generated.h"

/**
 * UDivineBeastsConfiguredGameplayAbility（基于数据定义的生肖技能基类）。
 * 保持 GAS 生命周期、预测/成本/冷却与实际玩法逻辑在原生技能框架内；
 * 仅提供从已经预加载的配置读取伤害数值与提交服务器伤害的安全入口。
 * 此类本身不包含具体生肖技能动作，不会在没有正式技能资产时自动释放伤害。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsConfiguredGameplayAbility
    : public UGamePlatformGameplayAbility
{
    GENERATED_BODY()

public:
    /** 与 AbilitySet 中真实授予的技能逻辑身份相对应的服务器安全主资产 ID。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability")
    FPrimaryAssetId AbilityDefinitionId;

    /**
     * 记录当前技能激活是否已经通过 GAS CommitAbility（正式消耗与冷却提交）。
     * 伤害必须在服务器有效激活且成功 Commit 后执行；多段技能在同一次激活中共用此资格。
     */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    /** 每个 InstancedPerActor 技能独立持有，结束/取消时立即清零，不复制。 */
    UPROPERTY(Transient)
    bool bCommittedForCurrentActivation = false;

public:

    /**
     * 当前技能等级从 GAS 获取；技能数值必须已在授权准备阶段使用 Gameplay Bundle 预加载。
     * 未预热、版本或编号不匹配均返回 false，不在技能施法/命中高频路径同步加载。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Ability")
    bool TryReadConfiguredBalance(
        FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const;


    /**
     * 服务器前向命中样板：从当前可信Avatar生成射线与目标命中事实，
     * 仅在GAS CommitAbility（成本与冷却提交）成功后使用项目技能数值的范围/基础伤害，
     * 不接收客户端指定的目标、射线或命中结果，也不另造平台伤害计算。
     * 可供子鼠等近距离单目标技能的正式实现复用；多目标/治疗/投射物需独立业务策略。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DivineBeasts|Ability")
    bool AuthorityTraceForwardAndApplyDamage(
        FGamePlatformCombatResult& OutResult, FString& OutError);
    /**
     * 仅由服务端已验证命中事实的具体技能实现调用；客户端不提供任意伤害 RPC。
     * 目标、世界、英雄身份和命中事实不一致时直接拒绝，不产生任何 GameplayEffect。
     * OutResult 是平台权威战斗组件原始结算，不复制算法或生成第二套伤害。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DivineBeasts|Ability")
    bool AuthorityApplyConfiguredDamage(
        AActor* Target, const FGamePlatformCombatHitContext& ValidatedHit,
        FGamePlatformCombatResult& OutResult, FString& OutError);
};
