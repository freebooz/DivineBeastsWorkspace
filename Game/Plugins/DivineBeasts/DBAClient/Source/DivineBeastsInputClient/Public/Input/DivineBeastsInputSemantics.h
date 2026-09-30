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
    /** 返回主攻击输入标签；下面五个访问器分别返回四个技能槽和目标锁定标签，调用前提同命名空间说明。 */
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AttackPrimary();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot1();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot2();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot3();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag AbilitySlot4();
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag TargetLock();

    /** 映射到GamePlatformAbilitySystem约定的Platform.Ability.Input子标签；TargetLock没有技能输入映射。 */
    DIVINEBEASTSINPUTCLIENT_API FGameplayTag ToAbilityInputTag(FGameplayTag SemanticTag);

    /** 返回Boolean + GameplayAction通道 + Passthrough策略的项目动作描述。 */
    DIVINEBEASTSINPUTCLIENT_API FGamePlatformInputSemanticDescriptor MakeGameplayActionDescriptor(
        FGameplayTag SemanticTag);

    /** 判断标签是否属于神兽联盟当前核心Gameplay输入集合。 */
    DIVINEBEASTSINPUTCLIENT_API bool IsCoreGameplaySemantic(FGameplayTag SemanticTag);
}
