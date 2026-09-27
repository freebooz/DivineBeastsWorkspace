#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformGameplayEligibilityProvider.generated.h"

UINTERFACE(MinimalAPI)
class UGamePlatformGameplayEligibilityProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * 平台级玩家玩法资格查询接口。
 * GamePlatformInteraction 等基础能力只读取资格，不拥有登录、准入、死亡或重生流程。
 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayEligibilityProvider
{
    GENERATED_BODY()

public:
    virtual bool IsServerPlayerActiveForGameplay() const = 0;
    virtual int32 GetGameplayAvatarGeneration() const = 0;
};
