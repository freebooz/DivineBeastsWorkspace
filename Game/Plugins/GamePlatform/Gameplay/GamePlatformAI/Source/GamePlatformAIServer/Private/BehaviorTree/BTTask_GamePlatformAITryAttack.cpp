#include "BehaviorTree/BTTask_GamePlatformAITryAttack.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Controllers/GamePlatformAIController.h"

UBTTask_GamePlatformAITryAttack::UBTTask_GamePlatformAITryAttack()
{
    NodeName = TEXT("GamePlatform AI Try Attack");
}

EBTNodeResult::Type UBTTask_GamePlatformAITryAttack::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    AGamePlatformAIController* Controller =
        Cast<AGamePlatformAIController>(
            OwnerComp.GetAIOwner());

    if (!Controller)
    {
        return EBTNodeResult::Failed;
    }

    return Controller->TryAttackCurrentTarget()
        ? EBTNodeResult::Succeeded
        : EBTNodeResult::Failed;
}
