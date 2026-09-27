#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformInteractionEligibilityProvider.generated.h"

UINTERFACE(MinimalAPI)
class UGamePlatformInteractionEligibilityProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Combat死亡、Stun、Cinematic等额外限制通过组合层实现本接口注入。
 * 不要求Interaction直接依赖Combat/Character/Input。
 */
class GAMEPLATFORMINTERACTION_API IGamePlatformInteractionEligibilityProvider
{
    GENERATED_BODY()

public:
    virtual bool IsEligibleForInteraction() const = 0;
};
