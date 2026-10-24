#include "Characters/DivineBeastsDeathDissolveComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "GameFramework/Actor.h"

void UDivineBeastsDeathDissolveComponent::BeginPlay()
{
    Super::BeginPlay();
    if(GetNetMode()==NM_DedicatedServer||!IsValid(GetOwner()))return;
    auto* Observed=GetOwner()->FindComponentByClass<UGamePlatformCombatComponent>();
    if(!IsValid(Observed))return;
    Combat=Observed;
    Subscription=Observed->OnCombatObservationChanged().AddUObject(this,&ThisClass::HandleStateChanged);
    // 初始已经死亡的实体只投影当前隐藏状态，避免迟到副本重播一次死亡。
    if(Observed->IsCombatDead()&&Observed->GetCombatAvatarGeneration()>0)
        StartDissolve(Observed->GetCombatAvatarGeneration(),FGamePlatformMeshDissolveSettings(),true);
    else HandleStateChanged();
}
void UDivineBeastsDeathDissolveComponent::HandleStateChanged()
{
    auto* State=Combat.Get();AActor* Owner=GetOwner();
    if(!IsValid(State)||!IsValid(Owner)||Owner->IsActorBeingDestroyed()||State->GetOwner()!=Owner||
        State->GetWorld()!=GetWorld())return;
    if(State->IsCombatDead()&&State->GetCombatAvatarGeneration()>0)
        StartDissolve(State->GetCombatAvatarGeneration(),FGamePlatformMeshDissolveSettings());
    else ResetDissolve();
}
void UDivineBeastsDeathDissolveComponent::NotifyVisualsChanged(AActor* Actor)
{
    if(IsValid(Actor))if(auto* Component=Actor->FindComponentByClass<UDivineBeastsDeathDissolveComponent>())
        Component->RefreshDissolveMeshes();
}
void UDivineBeastsDeathDissolveComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* State=Combat.Get())State->OnCombatObservationChanged().Remove(Subscription);
    Subscription.Reset();Combat.Reset();Super::EndPlay(Reason);
}
