#include "Components/GamePlatformQuestStateComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGamePlatformQuestStateComponent::UGamePlatformQuestStateComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformQuestStateComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(
        UGamePlatformQuestStateComponent,
        QuestSnapshots,
        COND_OwnerOnly);
}

void UGamePlatformQuestStateComponent::SetServerQuestSnapshots(
    const TArray<FGamePlatformQuestSnapshot>& InSnapshots)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    QuestSnapshots = InSnapshots;
    OnQuestSnapshotsChanged.Broadcast();
}

void UGamePlatformQuestStateComponent::OnRep_QuestSnapshots()
{
    OnQuestSnapshotsChanged.Broadcast();
}
