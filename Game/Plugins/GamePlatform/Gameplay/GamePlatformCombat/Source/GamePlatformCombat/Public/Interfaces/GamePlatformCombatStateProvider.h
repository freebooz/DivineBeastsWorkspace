#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformCombatStateProvider.generated.h"

UINTERFACE(MinimalAPI)
class UGamePlatformCombatStateProvider : public UInterface
{
    GENERATED_BODY()
};

/** 供上层读取生命与死亡的只读状态；临时护盾由GAS效果标签和权威结算事件表达。 */
class GAMEPLATFORMCOMBAT_API IGamePlatformCombatStateProvider
{
    GENERATED_BODY()

public:
    virtual float GetCombatHealth() const = 0;
    virtual float GetCombatMaxHealth() const = 0;
    virtual bool IsCombatDead() const = 0;
    virtual int32 GetCombatAvatarGeneration() const = 0;
};
