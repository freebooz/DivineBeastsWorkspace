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
    // 同一次OwnerOnly发布使用同一序列；目标进度更新即使尚未落库也会使客户端视图前进。
    const int64 PublishedSequence = ++SnapshotSequence;
    for (auto& Snapshot : QuestSnapshots) { Snapshot.SnapshotSequence = PublishedSequence; }
    OnQuestSnapshotsChanged.Broadcast();
}

void UGamePlatformQuestStateComponent::OnRep_QuestSnapshots()
{
    OnQuestSnapshotsChanged.Broadcast();
}
