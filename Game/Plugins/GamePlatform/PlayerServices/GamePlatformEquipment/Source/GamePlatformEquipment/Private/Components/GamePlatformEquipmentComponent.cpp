#include "Components/GamePlatformEquipmentComponent.h"

#include "Net/UnrealNetwork.h"

UGamePlatformEquipmentComponent::UGamePlatformEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UGamePlatformEquipmentComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(
        UGamePlatformEquipmentComponent,
        OwnerSnapshot,
        COND_OwnerOnly);

    DOREPLIFETIME(
        UGamePlatformEquipmentComponent,
        PublicSnapshot);

    DOREPLIFETIME(
        UGamePlatformEquipmentComponent,
        EquipmentRuntimeGeneration);
}

void UGamePlatformEquipmentComponent::SetServerSnapshots(
    const FGamePlatformEquipmentSnapshot& InOwnerSnapshot,
    int32 InRuntimeGeneration)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    OwnerSnapshot = InOwnerSnapshot;
    PublicSnapshot = BuildPublicSnapshot(InOwnerSnapshot);
    EquipmentRuntimeGeneration = FMath::Max(0, InRuntimeGeneration);

    OnOwnerEquipmentChanged.Broadcast();
    OnPublicEquipmentChanged.Broadcast();
}

void UGamePlatformEquipmentComponent::OnRep_OwnerSnapshot()
{
    OnOwnerEquipmentChanged.Broadcast();
}

void UGamePlatformEquipmentComponent::OnRep_PublicSnapshot()
{
    OnPublicEquipmentChanged.Broadcast();
}

FGamePlatformPublicEquipmentSnapshot
UGamePlatformEquipmentComponent::BuildPublicSnapshot(
    const FGamePlatformEquipmentSnapshot& Source)
{
    FGamePlatformPublicEquipmentSnapshot Result;
    Result.PublicStateRevision = Source.EquipmentRevision;
    Result.Slots.Reserve(Source.Slots.Num());

    for (const FGamePlatformEquipmentSlotState& Slot : Source.Slots)
    {
        FGamePlatformPublicEquipmentSlotState Public;
        Public.SlotId = Slot.SlotId;
        Public.EquipmentDefinitionId = Slot.EquipmentDefinitionId;
        Public.VisualDefinitionId = Slot.VisualDefinitionId;
        Result.Slots.Add(MoveTemp(Public));
    }

    return Result;
}
