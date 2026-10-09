#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UGamePlatformCombatFeedbackWorldSubsystem::DispatchConfirmedFeedback(
    const FGamePlatformCombatEvent& Event)
{
    // 普通UE委托绝不自动跨网络；这里只处理同一世界已确认且代次有效的事件。
    if (!GetWorld() || !Event.EventId.IsValid() || !IsValid(Event.TargetActor) ||
        Event.TargetActor->GetWorld() != GetWorld() ||
        Event.TargetAvatarGeneration <= 0)
    {
        return;
    }
    ConfirmedFeedback.Broadcast(Event);
}

void UGamePlatformCombatFeedbackWorldSubsystem::Deinitialize()
{
    ConfirmedFeedback.Clear();
    Super::Deinitialize();
}
