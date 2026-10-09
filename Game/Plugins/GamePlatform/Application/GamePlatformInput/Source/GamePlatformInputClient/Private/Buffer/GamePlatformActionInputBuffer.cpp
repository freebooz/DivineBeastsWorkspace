#include "Buffer/GamePlatformActionInputBuffer.h"

FGamePlatformActionInputBuffer::FGamePlatformActionInputBuffer(
    int32 InMaxEvents, double InLifetimeSeconds)
    : MaxEvents(FMath::Clamp(InMaxEvents, 1, 16))
    , LifetimeSeconds(FMath::Clamp(InLifetimeSeconds, 0.01, 0.5))
{
}

bool FGamePlatformActionInputBuffer::Enqueue(
    const FGamePlatformInputEvent& Event,
    double NowSeconds)
{
    // 只存本地平台已经发布的稳定动作输入，拒绝高频Triggered轴数据。
    const bool bDiscrete = Event.Phase == ETriggerEvent::Started ||
        Event.Phase == ETriggerEvent::Completed ||
        Event.Phase == ETriggerEvent::Canceled;
    const bool bPermittedEndReason =
        Event.EndReason == EGamePlatformInputEndReason::None ||
        Event.EndReason == EGamePlatformInputEndReason::NativeCompleted ||
        Event.EndReason == EGamePlatformInputEndReason::NativeCanceled;
    if (!FMath::IsFinite(NowSeconds) || !Event.SemanticId.IsValid() ||
        !bDiscrete || !bPermittedEndReason || Event.BindingGeneration == 0 ||
        Event.Sequence == 0)
    {
        return false;
    }

    PruneExpired(NowSeconds);
    if (!Pending.IsEmpty() &&
        Pending.Last().Event.BindingGeneration == Event.BindingGeneration &&
        Pending.Last().Event.Sequence >= Event.Sequence)
    {
        return false; // 同代次重复/乱序事件不得被二次消费。
    }

    if (Pending.Num() >= MaxEvents)
    {
        Pending.RemoveAt(0, 1, EAllowShrinking::No);
    }
    FPendingInput Item;
    Item.Event = Event;
    Item.ExpiresAtSeconds = NowSeconds + LifetimeSeconds;
    Pending.Add(MoveTemp(Item));
    return true;
}

int32 FGamePlatformActionInputBuffer::ConsumePending(
    double NowSeconds,
    uint64 ExpectedBindingGeneration,
    TArray<FGamePlatformInputEvent>& OutEvents)
{
    if (!FMath::IsFinite(NowSeconds) || ExpectedBindingGeneration == 0)
    {
        Reset();
        return 0;
    }
    PruneExpired(NowSeconds);
    int32 Consumed = 0;
    for (const FPendingInput& Item : Pending)
    {
        if (Item.Event.BindingGeneration == ExpectedBindingGeneration)
        {
            OutEvents.Add(Item.Event);
            ++Consumed;
        }
    }
    // 不保留旧代次或已经消费的事件，避免角色切换/重绑重复放招。
    Pending.Reset();
    return Consumed;
}

void FGamePlatformActionInputBuffer::PruneExpired(double NowSeconds)
{
    Pending.RemoveAll([NowSeconds](const FPendingInput& Item)
    {
        return Item.ExpiresAtSeconds < NowSeconds;
    });
}

void FGamePlatformActionInputBuffer::Reset()
{
    Pending.Reset();
}
