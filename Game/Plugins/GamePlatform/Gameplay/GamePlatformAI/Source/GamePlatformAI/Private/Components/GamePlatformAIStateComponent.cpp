#include "Components/GamePlatformAIStateComponent.h"

#include "Net/UnrealNetwork.h"

UGamePlatformAIStateComponent::UGamePlatformAIStateComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformAIStateComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner() && GetOwner()->HasAuthority() &&
        !Snapshot.AIEntityId.IsValid())
    {
        Snapshot.AIEntityId = FGuid::NewGuid();
        Snapshot.AIInstanceGeneration =
            FMath::Max(1, Snapshot.AIInstanceGeneration);
        TouchRevision();
    }
}

void UGamePlatformAIStateComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGamePlatformAIStateComponent, Snapshot);
}

void UGamePlatformAIStateComponent::SetDefinitionAsset(
    TSoftObjectPtr<UGamePlatformAIDefinition> InDefinition)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        DefinitionAsset = InDefinition;
    }
}

void UGamePlatformAIStateComponent::InitializeServerState(
    FName DefinitionId,
    int32 Generation)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    if (!Snapshot.AIEntityId.IsValid())
    {
        Snapshot.AIEntityId = FGuid::NewGuid();
    }

    Snapshot.AIDefinitionId = DefinitionId;
    Snapshot.AIInstanceGeneration = FMath::Max(1, Generation);
    Snapshot.CurrentTargetEntityId.Invalidate();
    Snapshot.PublicState = EGamePlatformAIPublicState::Idle;
    Snapshot.MovementIntentTag = FGameplayTag();
    Snapshot.CombatIntentTag = FGameplayTag();
    TouchRevision();
}

void UGamePlatformAIStateComponent::SetServerPublicState(
    EGamePlatformAIPublicState NewState)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() ||
        Snapshot.PublicState == NewState)
    {
        return;
    }

    Snapshot.PublicState = NewState;
    TouchRevision();
}

void UGamePlatformAIStateComponent::SetServerTargetEntityId(
    const FGuid& TargetEntityId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() ||
        Snapshot.CurrentTargetEntityId == TargetEntityId)
    {
        return;
    }

    Snapshot.CurrentTargetEntityId = TargetEntityId;
    TouchRevision();
}

void UGamePlatformAIStateComponent::SetServerIntentTags(
    const FGameplayTag& MovementIntent,
    const FGameplayTag& CombatIntent)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    if (Snapshot.MovementIntentTag == MovementIntent &&
        Snapshot.CombatIntentTag == CombatIntent)
    {
        return;
    }

    Snapshot.MovementIntentTag = MovementIntent;
    Snapshot.CombatIntentTag = CombatIntent;
    TouchRevision();
}

void UGamePlatformAIStateComponent::AdvanceServerGeneration()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    ++Snapshot.AIInstanceGeneration;
    Snapshot.CurrentTargetEntityId.Invalidate();
    Snapshot.PublicState = EGamePlatformAIPublicState::Disabled;
    Snapshot.MovementIntentTag = FGameplayTag();
    Snapshot.CombatIntentTag = FGameplayTag();
    TouchRevision();
}

void UGamePlatformAIStateComponent::OnRep_Snapshot()
{
    OnStateChanged.Broadcast(Snapshot);
}

void UGamePlatformAIStateComponent::TouchRevision()
{
    ++Snapshot.StateRevision;
    OnStateChanged.Broadcast(Snapshot);
}
