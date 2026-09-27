#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformCombatant.generated.h"

class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatComponent;

UINTERFACE(MinimalAPI)
class UGamePlatformCombatant : public UInterface
{
    GENERATED_BODY()
};

/** 角色、NPC和测试Dummy都可以实现的中立战斗参与者接口。 */
class GAMEPLATFORMCOMBAT_API IGamePlatformCombatant
{
    GENERATED_BODY()

public:
    virtual UGamePlatformAbilitySystemComponent* GetGamePlatformAbilitySystemComponent() const = 0;
    virtual UGamePlatformCombatComponent* GetGamePlatformCombatComponent() const = 0;
    virtual bool CanSourceCombat() const = 0;
    virtual bool CanReceiveCombat() const = 0;
    virtual int32 GetCombatAvatarGeneration() const = 0;
};
