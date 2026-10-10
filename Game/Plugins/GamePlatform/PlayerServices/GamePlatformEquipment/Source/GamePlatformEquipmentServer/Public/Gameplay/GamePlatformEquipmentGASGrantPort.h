// 服务器权威装备契约；仅服务器调用，持久化/资格由注入Port负责；组件退出撤销自有GAS句柄，外部资源不归组件。
#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UGamePlatformEquipmentDefinition;

class GAMEPLATFORMEQUIPMENTSERVER_API IGamePlatformEquipmentGameplayAssetResolver
{
public:
    virtual ~IGamePlatformEquipmentGameplayAssetResolver() = default;

    /** 仅服务器游戏线程：稳定能力集合身份非None；成功完整填写能力/效果Class，失败不得输出伪造空成功；资源由外部解析器拥有。 */
    virtual bool ResolveAbilitySet(
        FName AbilitySetDefinitionId,
        TArray<TSubclassOf<UGameplayAbility>>& OutAbilities,
        TArray<TSubclassOf<UGameplayEffect>>& OutEffects) = 0;

    /** 仅服务器游戏线程：非None效果定义身份；true返回有效Class，false表示缺失/不可用，不自行生成替身。 */
    virtual bool ResolveGameplayEffect(
        FName GameplayEffectDefinitionId,
        TSubclassOf<UGameplayEffect>& OutEffect) = 0;
};

struct GAMEPLATFORMEQUIPMENTSERVER_API FGamePlatformEquipmentGameplayGrantHandle
{
    /** 单次授予随机身份；失效Guid表示无资源所有权，不能由外部伪造用于撤销。 */
    FGuid GrantId;
    /** 本次Grant授予的能力句柄集合；只撤销集合内条目，不碰其他来源能力。 */
    TArray<FGameplayAbilitySpecHandle> AbilityHandles;
    /** 本次Grant应用的持续效果句柄；随Revoke/组件退出释放。 */
    TArray<FActiveGameplayEffectHandle> EffectHandles;

    bool IsValid() const
    {
        return GrantId.IsValid();
    }

    void Reset()
    {
        GrantId.Invalidate();
        AbilityHandles.Reset();
        EffectHandles.Reset();
    }
};

class GAMEPLATFORMEQUIPMENTSERVER_API FGamePlatformEquipmentGASGrantPort
{
public:
    explicit FGamePlatformEquipmentGASGrantPort(
        TSharedPtr<
            IGamePlatformEquipmentGameplayAssetResolver,
            ESPMode::ThreadSafe> InResolver);

    /** IsScopeCurrent为空时只检查ASC；组件提供寿命/Avatar探针，每次外部解析和GAS通知后检查，失效撤回本次候选。
     * 新增默认参数保持源码调用兼容，改变导出符号，消费者须重新编译。仅服务器游戏线程：有效权威ASC和结构合法Definition；true产生唯一OutHandle所有权，失败撤销本次已授予资源，不撤销其他来源能力。 */
    bool Grant(
        UAbilitySystemComponent* AbilitySystem,
        const UGamePlatformEquipmentDefinition& Definition,
        FGamePlatformEquipmentGameplayGrantHandle& OutHandle,
        TFunction<bool()> IsScopeCurrent = {}) const;

    /** 仅服务器游戏线程：撤销Handle列出的本Port自有能力/效果后失效Handle；重复撤销安全，不清空ASC全部能力。 */
    void Revoke(
        UAbilitySystemComponent* AbilitySystem,
        FGamePlatformEquipmentGameplayGrantHandle& Handle) const;

private:
    TSharedPtr<
        IGamePlatformEquipmentGameplayAssetResolver,
        ESPMode::ThreadSafe> Resolver;
};
