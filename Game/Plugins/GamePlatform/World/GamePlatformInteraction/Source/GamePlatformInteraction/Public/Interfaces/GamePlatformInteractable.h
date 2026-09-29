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

    /**
     * Custom（自定义提交）默认采用 Fail-Closed（失败关闭）策略。
     * 具体目标必须显式实现提交副作用；禁止未实现处理器时静默返回成功，避免“交互已完成但业务状态未改变”。
     */
    virtual bool CommitInteraction(
        const FGamePlatformInteractionSession& Session,
        const FGamePlatformInteractionOption& Option)
    {
        return false;
    }
};
