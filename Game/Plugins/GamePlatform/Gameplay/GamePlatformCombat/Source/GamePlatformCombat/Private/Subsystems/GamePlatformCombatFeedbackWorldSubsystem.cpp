#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"
#include "Feedback/GamePlatformCombatFeedbackDedupePolicy.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UGamePlatformCombatFeedbackWorldSubsystem::DispatchConfirmedFeedback(
    const FGamePlatformCombatEvent& Event)
{
    // 普通UE委托绝不自动跨网络。这里只处理当前客户端世界内已确认、
    // 代次和数字均合法的可选表现事实；绝不在此修改真实角色生命。
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer ||
        !Event.EventId.IsValid() || !IsValid(Event.TargetActor) ||
        Event.TargetActor->GetWorld() != World ||
        Event.TargetAvatarGeneration <= 0 || Event.WorldContextGeneration <= 0 ||
        !FMath::IsFinite(Event.AppliedMagnitude) ||
        !FMath::IsFinite(Event.AppliedToShield) ||
        Event.AppliedMagnitude < 0.0f || Event.AppliedToShield < 0.0f ||
        (Event.EventType == EGamePlatformCombatEventType::Damage &&
         Event.AppliedToShield > Event.AppliedMagnitude))
    {
        return;
    }
    if (Event.EventType != EGamePlatformCombatEventType::Damage &&
        Event.EventType != EGamePlatformCombatEventType::Healing &&
        Event.EventType != EGamePlatformCombatEventType::ControlApplied &&
        Event.EventType != EGamePlatformCombatEventType::Death)
    {
        return;
    }

    // 消息总线直接调用与UE自动化测试完全相同的纯去重策略。
    // 同一Guid的Damage与Death必须分别展示，同时避免重复的Unreliable事件刷屏。
    if (!GamePlatformCombatFeedbackDedupe::FCache::TryRememberState(
            SeenFeedbackTypes, FeedbackRing, NextFeedbackSlot,
            Event.EventId, Event.EventType, MaxRememberedFeedback))
    {
        return;
    }
    ConfirmedFeedback.Broadcast(Event);
}

void UGamePlatformCombatFeedbackWorldSubsystem::Deinitialize()
{
    ConfirmedFeedback.Clear();
    SeenFeedbackTypes.Reset();
    FeedbackRing.Reset();
    NextFeedbackSlot = 0;
    Super::Deinitialize();
}
