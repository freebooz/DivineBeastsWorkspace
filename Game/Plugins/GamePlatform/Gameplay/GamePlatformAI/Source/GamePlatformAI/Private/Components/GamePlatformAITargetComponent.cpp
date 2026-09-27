#include "Components/GamePlatformAITargetComponent.h"

#include "Net/UnrealNetwork.h"

UGamePlatformAITargetComponent::UGamePlatformAITargetComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformAITargetComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner() && GetOwner()->HasAuthority() &&
        !TargetEntityId.IsValid())
    {
        TargetEntityId = FGuid::NewGuid();
        TargetGeneration = FMath::Max(1, TargetGeneration);
    }
}

void UGamePlatformAITargetComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGamePlatformAITargetComponent, TargetEntityId);
    DOREPLIFETIME(UGamePlatformAITargetComponent, TargetGeneration);
    DOREPLIFETIME(UGamePlatformAITargetComponent, bAITargetEnabled);
}

void UGamePlatformAITargetComponent::SetAITargetEnabled(bool bEnabled)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bAITargetEnabled = bEnabled;
    }
}

void UGamePlatformAITargetComponent::AdvanceTargetGeneration()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ++TargetGeneration;
    }
}
