// 平台双端Actor资格复制组件：服务器游戏线程写Active/正整数代次，客户端只读；不拥有登录、出生或竞技流程。
#include "Components/GamePlatformGameplayEligibilityComponent.h"

#include "Net/UnrealNetwork.h"
// HasAuthority直接读取Owner，非Unity编译也必须显式提供Actor完整类型。
#include "GameFramework/Actor.h"

UGamePlatformGameplayEligibilityComponent::
UGamePlatformGameplayEligibilityComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformGameplayEligibilityComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(
        UGamePlatformGameplayEligibilityComponent,
        bServerPlayerActive);
    DOREPLIFETIME(
        UGamePlatformGameplayEligibilityComponent,
        AvatarGeneration);
}

void UGamePlatformGameplayEligibilityComponent::SetServerPlayerActive(
    bool bActive)
{
    if (GetOwner() && GetOwner()->HasAuthority() && bServerPlayerActive != bActive)
    {
        bServerPlayerActive = bActive;
        BroadcastSnapshot();
    }
}

void UGamePlatformGameplayEligibilityComponent::AdvanceAvatarGeneration()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // 达到上限立即失活，禁止有符号溢出让旧句柄重新合法。
        if (AvatarGeneration == MAX_int32) { SetServerPlayerActive(false); return; }
        ++AvatarGeneration;
        BroadcastSnapshot();
    }
}

bool UGamePlatformGameplayEligibilityComponent::BindServerAvatarGeneration(int32 NewAvatarGeneration)
{
    check(IsInGameThread());
    if (!GetOwner() || !GetOwner()->HasAuthority() || bServerPlayerActive ||
        NewAvatarGeneration <= 0 || NewAvatarGeneration < AvatarGeneration) { return false; }
    if (NewAvatarGeneration != AvatarGeneration) { AvatarGeneration = NewAvatarGeneration; BroadcastSnapshot(); }
    return true;
}

void UGamePlatformGameplayEligibilityComponent::OnRep_ServerPlayerActive()
{
    BroadcastSnapshot();
}

void UGamePlatformGameplayEligibilityComponent::OnRep_AvatarGeneration()
{
    BroadcastSnapshot();
}

void UGamePlatformGameplayEligibilityComponent::BroadcastSnapshot()
{
    EligibilityChanged.Broadcast(GetSnapshot());
}
