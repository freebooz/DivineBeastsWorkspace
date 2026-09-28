#pragma once

#include "Abilities/GameplayAbility.h"
#include "GamePlatformGameplayAbility.generated.h"

/**
 * UGamePlatformGameplayAbility（游戏平台技能基类）。
 *
 * 该类型是 GamePlatformAbilitySystem（平台技能系统）公开 AbilitySet 契约要求的稳定基类，
 * 只提供跨游戏通用的 GAS（Gameplay Ability System，玩法技能系统）类型边界，
 * 不包含具体游戏、生肖、MOBA 或项目表现逻辑。
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
};
