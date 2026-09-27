#pragma once

#include "CoreMinimal.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Identity/DivineBeastsZodiacIdentity.h"

/**
 * IDivineBeastsCharacterCreationProvider（神兽联盟角色创建提供者）。
 * 供ApplicationFlow/UI通过平台Modular Feature（模块化特性）组合使用；
 * 本接口不执行后端创建、Entitlement最终校验或持久化。
 */
class DIVINEBEASTSCHARACTERSRUNTIME_API IDivineBeastsCharacterCreationProvider
    : public IGamePlatformCharacterCreationProvider
{
public:
    virtual bool TryGetZodiacIdentity(
        FName HeroDefinitionId,
        EDivineBeastsZodiacIdentity& OutZodiacIdentity) const = 0;
};
