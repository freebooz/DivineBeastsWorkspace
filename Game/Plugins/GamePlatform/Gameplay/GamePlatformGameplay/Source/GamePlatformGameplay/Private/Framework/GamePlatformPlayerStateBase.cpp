// 平台公开玩家状态及姓名的事件桥；不新增复制字段，不读取项目档案或主动创建UI。
#include "Framework/GamePlatformPlayerStateBase.h"

#include "Net/UnrealNetwork.h"

void AGamePlatformPlayerStateBase::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();
    OnPublicPlayerNameChanged.Broadcast();
}

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
