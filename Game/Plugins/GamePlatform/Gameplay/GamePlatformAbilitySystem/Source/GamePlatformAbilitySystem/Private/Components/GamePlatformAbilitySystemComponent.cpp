#include "Components/GamePlatformAbilitySystemComponent.h"

UGamePlatformAbilitySystemComponent::UGamePlatformAbilitySystemComponent()
{
    SetIsReplicatedByDefault(true);
    // Mixed（混合复制）是多人玩家ASC的通用默认值：Owner接收完整GameplayEffect，其他客户端只接收必要Cue/Tag。
    // AI或纯服务端派生ASC可按场景进一步设置为Minimal，避免平台层把所有实例锁死为Full高带宽模式。
    SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

bool UGamePlatformAbilitySystemComponent::BindAbilityActorInfo(
    AActor* InOwnerActor,
    AActor* InAvatarActor)
{
    if (!IsValid(InOwnerActor) || !IsValid(InAvatarActor))
    {
        return false;
    }

    if (AbilityActorInfo.IsValid() &&
        AbilityActorInfo->OwnerActor.Get() == InOwnerActor &&
        AbilityActorInfo->AvatarActor.Get() == InAvatarActor)
    {
        return true;
    }

    InitAbilityActorInfo(InOwnerActor, InAvatarActor);
    ++AvatarGeneration;
    BroadcastAvatarBinding();
    return true;
}

void UGamePlatformAbilitySystemComponent::ClearAbilityAvatar()
{
    if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    ClearActorInfo();
    ++AvatarGeneration;
    BroadcastAvatarBinding();
}

void UGamePlatformAbilitySystemComponent::BroadcastAvatarBinding()
{
    AvatarBindingChanged.Broadcast(GetAvatarBindingSnapshot());
}
