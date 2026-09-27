#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GamePlatformAITryAttack.generated.h"

UCLASS()
class GAMEPLATFORMAISERVER_API UBTTask_GamePlatformAITryAttack final
    : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_GamePlatformAITryAttack();

    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;
};
