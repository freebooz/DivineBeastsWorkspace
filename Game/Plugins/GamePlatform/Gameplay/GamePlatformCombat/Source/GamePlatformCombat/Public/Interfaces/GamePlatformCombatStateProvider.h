#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformCombatStateProvider.generated.h"

UINTERFACE(MinimalAPI)
class UGamePlatformCombatStateProvider : public UInterface
{
    GENERATED_BODY()
};

/** 供上层读取可复制战斗快照，不暴露修改入口。 */
class GAMEPLATFORMCOMBAT_API IGamePlatformCombatStateProvider
{
    GENERATED_BODY()

public:
    virtual float GetCombatHealth() const = 0;
    virtual float GetCombatMaxHealth() const = 0;
    virtual float GetCombatShield() const = 0;
    virtual float GetCombatMaxShield() const = 0;
    virtual bool IsCombatDead() const = 0;
    virtual int32 GetCombatAvatarGeneration() const = 0;
};
