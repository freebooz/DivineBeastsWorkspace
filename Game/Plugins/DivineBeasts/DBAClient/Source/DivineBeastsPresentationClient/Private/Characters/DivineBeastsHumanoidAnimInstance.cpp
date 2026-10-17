// SSLM人型角色死亡动画只监听本Pawn的Combat快照；失活时移除订阅。
#include "Characters/DivineBeastsHumanoidAnimInstance.h"

#include "Components/GamePlatformCombatComponent.h"
#include "GameFramework/Pawn.h"

void UDivineBeastsHumanoidAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    ReleaseCombatSubscription();
    bCombatDead = false;
    APawn* Pawn = TryGetPawnOwner();
    auto* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (!IsValid(Combat) || Combat->GetOwner() != Pawn) return;
    ObservedCombat = Combat;
    CombatSubscription = Combat->OnCombatObservationChanged().AddUObject(
        this, &ThisClass::HandleCombatChanged);
    HandleCombatChanged();
}

void UDivineBeastsHumanoidAnimInstance::NativeUninitializeAnimation()
{
    ReleaseCombatSubscription();
    Super::NativeUninitializeAnimation();
}

void UDivineBeastsHumanoidAnimInstance::HandleCombatChanged()
{
    auto* Combat = ObservedCombat.Get();
    APawn* Pawn = TryGetPawnOwner();
    if (!IsValid(Combat) || !IsValid(Pawn) || Combat->GetOwner() != Pawn ||
        Combat->GetWorld() != GetWorld() || Pawn->IsActorBeingDestroyed()) return;
    bCombatDead = Combat->IsCombatDead();
}

void UDivineBeastsHumanoidAnimInstance::ReleaseCombatSubscription()
{
    if (auto* Combat = ObservedCombat.Get())
        Combat->OnCombatObservationChanged().Remove(CombatSubscription);
    CombatSubscription.Reset();
    ObservedCombat.Reset();
}
