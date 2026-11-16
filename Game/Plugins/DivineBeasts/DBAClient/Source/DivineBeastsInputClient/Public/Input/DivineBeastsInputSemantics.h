#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformInputTypes.h"

/**
 * DivineBeastsInputSemantics（神兽联盟输入语义）。
 * 这些标签属于项目层，禁止回灌GamePlatformInput；平台只识别Descriptor合同。
 * 标签访问与映射须在引擎UObject/配置初始化完成后的游戏线程调用，禁止用于全局静态初始化。
 * 值由项目GameplayTags配置定义，首次访问后缓存；缺失标签报告配置错误，不注册临时替代标签。
 */
namespace DivineBeastsInputSemantics
{
    /** 返回基础攻击输入；其余访问器含Slot1..Slot7七个可用技能位及目标锁定。 */
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AttackPrimary();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot1();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot2();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot3();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot4();
    /** 八槽扩展；与现有Slot1..Slot4共享同一语义映射、ASC输入令牌和客户端生命周期。 */
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot5();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot6();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot7();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag TargetLock();

    /** 映射到GamePlatformAbilitySystem约定的Platform.Ability.Input子标签；TargetLock没有技能输入映射。 */
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag ToAbilityInputTag(FGameplayTag SemanticTag);

    /** 返回Boolean + GameplayAction通道 + Passthrough策略的项目动作描述。 */
    DIVINEBEASTSINPUTCLIENT_API FGamePlatformInputSemanticDescriptor MakeGameplayActionDescriptor(
        FGameplayTag SemanticTag);

    /** 判断标签是否属于神兽联盟当前核心Gameplay输入集合。 */
    DIVINEBEASTSINPUTCLIENT_API bool IsCoreGameplaySemantic(FGameplayTag SemanticTag);
}
