#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformInputTypes.h"

/**
 * DivineBeastsInputSemantics（神兽联盟输入语义）。
 * 这些标签属于项目层，禁止回灌GamePlatformInput；平台只识别Descriptor合同。
 */
namespace DivineBeastsInputSemantics
{
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
