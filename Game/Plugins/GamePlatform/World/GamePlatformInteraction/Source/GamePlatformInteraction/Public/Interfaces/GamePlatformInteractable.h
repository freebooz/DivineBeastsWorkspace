#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/GamePlatformInteractionOption.h"
#include "GamePlatformInteractable.generated.h"

class UGamePlatformInteractableComponent;
struct FGamePlatformInteractionSession;

UINTERFACE(MinimalAPI)
class UGamePlatformInteractable : public UInterface
{
    GENERATED_BODY()
};

/** 具体Door/Pickup/Harvest/未来NPC可实现的中立交互行为契约。 */
class GAMEPLATFORMINTERACTION_API IGamePlatformInteractable
{
    GENERATED_BODY()

public:
    virtual bool CanBeginInteraction(
        const FGamePlatformInteractionSession& Session,
        const FGamePlatformInteractionOption& Option) const
    {
        return true;
    }

    virtual bool CommitInteraction(
        const FGamePlatformInteractionSession& Session,
        const FGamePlatformInteractionOption& Option)
    {
        return true;
    }
};
