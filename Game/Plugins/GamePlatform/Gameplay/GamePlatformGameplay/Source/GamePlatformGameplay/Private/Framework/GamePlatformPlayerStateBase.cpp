#include "Framework/GamePlatformPlayerStateBase.h"

#include "Net/UnrealNetwork.h"

void AGamePlatformPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGamePlatformPlayerStateBase, Snapshot);
}

void AGamePlatformPlayerStateBase::OnRep_Snapshot()
{
    // 当前平台层只提供值快照，不在PlayerState内部建立第二套事件状态机。
    // 客户端消费者应在自己的组合根按复制后的StateRevision幂等重算。
}

void AGamePlatformPlayerStateBase::Publish(const FGamePlatformPlayerLifecycleSnapshot& Value)
{
    if (!HasAuthority())
    {
        return;
    }

    Snapshot = Value;
    ForceNetUpdate();
}
