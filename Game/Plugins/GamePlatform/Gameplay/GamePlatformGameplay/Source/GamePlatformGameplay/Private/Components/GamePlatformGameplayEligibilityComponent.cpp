#include "Components/GamePlatformGameplayEligibilityComponent.h"

#include "Net/UnrealNetwork.h"

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
        ++AvatarGeneration;
        BroadcastSnapshot();
    }
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
