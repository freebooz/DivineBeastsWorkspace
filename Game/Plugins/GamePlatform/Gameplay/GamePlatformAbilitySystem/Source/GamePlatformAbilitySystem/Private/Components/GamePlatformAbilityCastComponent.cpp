// 平台中立表现窗口执行：有限单次Timer，网络状态复制；无需Tick或任何动画/VFX播放器。
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UGamePlatformAbilityCastComponent::UGamePlatformAbilityCastComponent()
{ PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }

bool UGamePlatformAbilityCastComponent::PublishConfirmedCast(FGuid ActivationId, FName AbilityId,
    FName PresentationStyleId, int32 AvatarGeneration, float DurationSeconds)
{
    UWorld* World=GetWorld(); AActor* Owner=GetOwner();
    if (!IsInGameThread() || bClosing || !IsValid(Owner) || !Owner->HasAuthority() || Owner->IsActorBeingDestroyed() ||
        !World || World->bIsTearingDown || !ActivationId.IsValid() || ActivationId==Facts.ActivationId ||
        AbilityId.IsNone() || PresentationStyleId.IsNone() || AvatarGeneration<=0 ||
        !FMath::IsFinite(DurationSeconds) || DurationSeconds<=0 || DurationSeconds>10) return false;
    World->GetTimerManager().ClearTimer(ExpiryTimer);
    Facts.ActivationId=ActivationId; Facts.AbilityId=AbilityId; Facts.PresentationStyleId=PresentationStyleId;
    Facts.AvatarGeneration=AvatarGeneration; Facts.DurationSeconds=DurationSeconds; Facts.bActive=true;
    const auto* State=World->GetGameState();
    Facts.StartedServerSeconds=State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    // 外部通知前先发布所有权并启动Timer，重入取消/后继发布能完整撤销本次计时器。
    World->GetTimerManager().SetTimer(ExpiryTimer,FTimerDelegate::CreateWeakLambda(this,[this,ActivationId]()
        { CancelConfirmedCast(ActivationId); }),DurationSeconds,false);
    Owner->ForceNetUpdate();
    OnRep_Facts(); return true;
}

bool UGamePlatformAbilityCastComponent::CancelConfirmedCast(FGuid ActivationId)
{
    AActor* Owner=GetOwner();
    if (!IsInGameThread() || bClosing || !IsValid(Owner) || !Owner->HasAuthority() ||
        !Facts.bActive || !ActivationId.IsValid() || Facts.ActivationId!=ActivationId) return false;
    if (UWorld* World=GetWorld()) World->GetTimerManager().ClearTimer(ExpiryTimer);
    Facts.bActive=false; Owner->ForceNetUpdate(); OnRep_Facts(); return true;
}

void UGamePlatformAbilityCastComponent::OnRep_Facts()
{
    if (bClosing) return;
    const auto Snapshot=Facts; // 同步消费者改变Facts时，后续消费者仍读取同一次发布的值快照。
    Changed.Broadcast(this,Snapshot);
}
void UGamePlatformAbilityCastComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UGamePlatformAbilityCastComponent,Facts); }
void UGamePlatformAbilityCastComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bClosing=true;
    if (UWorld* World=GetWorld()) World->GetTimerManager().ClearTimer(ExpiryTimer);
    Facts.bActive=false; Changed.Clear(); Super::EndPlay(Reason);
}
