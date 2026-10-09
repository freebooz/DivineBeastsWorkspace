// 平台共享装备复制组件：服务器提交拥有者完整投影与公开外观投影，客户端通过OnRep事件读取；不持有GAS授予或持久化事务。
// 组件生命周期由Actor拥有，状态提交须服务器权威；NoPCH下显式读取Actor完整定义，不依赖其他编译单元的传递头。
#include "Components/GamePlatformEquipmentComponent.h"

#include "GameFramework/Actor.h"
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
