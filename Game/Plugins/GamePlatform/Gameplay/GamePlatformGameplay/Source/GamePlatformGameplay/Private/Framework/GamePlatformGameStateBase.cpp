#include "Framework/GamePlatformGameStateBase.h"

#include "Components/GamePlatformExperienceComponent.h"

AGamePlatformGameStateBase::AGamePlatformGameStateBase()
{
    bReplicates = true;
    Experience = CreateDefaultSubobject<UGamePlatformExperienceComponent>(TEXT("Experience"));
}

bool AGamePlatformGameStateBase::HasSynchronizedServerTime() const
{
    return GetNetMode() != NM_Client || bHasReceivedServerTime;
}

FDelegateHandle AGamePlatformGameStateBase::AddServerTimeSynchronizedHandler(
    FGamePlatformServerTimeSynchronized::FDelegate Handler)
{
    check(IsInGameThread());
    return Handler.IsBound() ? ServerTimeSynchronized.Add(MoveTemp(Handler)) : FDelegateHandle();
}

void AGamePlatformGameStateBase::RemoveServerTimeSynchronizedHandler(const FDelegateHandle Handle)
{
    check(IsInGameThread());
    if (Handle.IsValid()) ServerTimeSynchronized.Remove(Handle);
}

void AGamePlatformGameStateBase::OnRep_ReplicatedWorldTimeSecondsDouble()
{
    Super::OnRep_ReplicatedWorldTimeSecondsDouble();
    bHasReceivedServerTime = true;
    // 在引擎完成均值/平滑校正后发布，不复制另一套时钟，不在客户端周期查询时差。
    ServerTimeSynchronized.Broadcast(GetServerWorldTimeSeconds());
}

void AGamePlatformGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ServerTimeSynchronized.Clear();
    bHasReceivedServerTime = false;
    Super::EndPlay(EndPlayReason);
}
