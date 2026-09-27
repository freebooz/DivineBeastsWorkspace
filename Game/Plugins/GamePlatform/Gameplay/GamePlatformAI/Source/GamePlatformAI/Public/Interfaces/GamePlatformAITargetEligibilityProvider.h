#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformAITargetEligibilityProvider.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UGamePlatformAITargetEligibilityProvider : public UInterface
{
    GENERATED_BODY()
};

class GAMEPLATFORMAI_API IGamePlatformAITargetEligibilityProvider
{
    GENERATED_BODY()

public:
    virtual bool IsEligibleAsAITarget(const AActor* RequestingAI) const = 0;
    virtual FGuid GetAITargetEntityId() const = 0;
    virtual int32 GetAITargetGeneration() const = 0;
};
