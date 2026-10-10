#include "Framework/GamePlatformPlayerStateBase.h"

#include "Net/UnrealNetwork.h"

void AGamePlatformPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGamePlatformPlayerStateBase, Snapshot);
}

void AGamePlatformPlayerStateBase::OnRep_Snapshot()
{
    OnLifecycleChanged.Broadcast();
}

void AGamePlatformPlayerStateBase::Publish(const FGamePlatformPlayerLifecycleSnapshot& Value)
{
    if (!HasAuthority())
    {
        return;
    }

    Snapshot = Value;
    OnLifecycleChanged.Broadcast();
    ForceNetUpdate();
}
