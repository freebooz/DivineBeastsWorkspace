#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"

#include "AbilitySystemComponent.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"

namespace
{
bool IsEquivalentStatus(
    const FDivineBeastsPlayerStatusViewData& A,
    const FDivineBeastsPlayerStatusViewData& B)
{
    return FMath::IsNearlyEqual(A.Health, B.Health, 0.0001) &&
        FMath::IsNearlyEqual(A.MaxHealth, B.MaxHealth, 0.0001) &&
        FMath::IsNearlyEqual(A.Energy, B.Energy, 0.0001) &&
        FMath::IsNearlyEqual(A.MaxEnergy, B.MaxEnergy, 0.0001) &&
        FMath::IsNearlyEqual(A.Momentum, B.Momentum, 0.0001) &&
        FMath::IsNearlyEqual(A.MaxMomentum, B.MaxMomentum, 0.0001) &&
        A.bDead == B.bDead;
}
}

bool UDivineBeastsPlayerStatusViewModel::BindToAbilitySystem(UAbilitySystemComponent* InAbilitySystem)
{
    UnbindFromAbilitySystem();
    if (!IsValid(InAbilitySystem))
    {
        return false;
    }

    AbilitySystem = InAbilitySystem;
    HealthHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetHealthAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);
    MaxHealthHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);
    EnergyHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetEnergyAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);
    MaxEnergyHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetMaxEnergyAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);
    MomentumHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);
    MaxMomentumHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UDivineBeastsMomentumAttributeSet::GetMaxMomentumAttribute()).AddUObject(
            this, &UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged);

    RefreshSnapshot(true);
    return true;
}

void UDivineBeastsPlayerStatusViewModel::UnbindFromAbilitySystem()
{
    if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
    {
        if (HealthHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
        }
        if (MaxHealthHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
        }
        if (EnergyHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetEnergyAttribute()).Remove(EnergyHandle);
        }
        if (MaxEnergyHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxEnergyAttribute()).Remove(MaxEnergyHandle);
        }
        if (MomentumHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()).Remove(MomentumHandle);
        }
        if (MaxMomentumHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(UDivineBeastsMomentumAttributeSet::GetMaxMomentumAttribute()).Remove(MaxMomentumHandle);
        }
    }

    AbilitySystem.Reset();
    HealthHandle.Reset();
    MaxHealthHandle.Reset();
    EnergyHandle.Reset();
    MaxEnergyHandle.Reset();
    MomentumHandle.Reset();
    MaxMomentumHandle.Reset();
}

void UDivineBeastsPlayerStatusViewModel::BeginDestroy()
{
    UnbindFromAbilitySystem();
    Super::BeginDestroy();
}

void UDivineBeastsPlayerStatusViewModel::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
    RefreshSnapshot(false);
}

void UDivineBeastsPlayerStatusViewModel::RefreshSnapshot(bool bForceBroadcast)
{
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (!ASC)
    {
        return;
    }

    FDivineBeastsPlayerStatusViewData NewStatus;
    if (const UGamePlatformCombatAttributeSet* Combat = ASC->GetSet<UGamePlatformCombatAttributeSet>())
    {
        NewStatus.Health = Combat->GetHealth();
        NewStatus.MaxHealth = Combat->GetMaxHealth();
        // 蓝条读取通用独立能量；金色气势从另一项目属性集读取。
        NewStatus.Energy = Combat->GetEnergy();
        NewStatus.MaxEnergy = Combat->GetMaxEnergy();
        NewStatus.bDead = Combat->GetMaxHealth() > 0.0f && Combat->GetHealth() <= 0.0f;
    }
    if (const UDivineBeastsMomentumAttributeSet* Momentum = ASC->GetSet<UDivineBeastsMomentumAttributeSet>())
    {
        NewStatus.Momentum = Momentum->GetMomentum();
        NewStatus.MaxMomentum = Momentum->GetMaxMomentum();
    }

    if (!bForceBroadcast && IsEquivalentStatus(Status, NewStatus))
    {
        return;
    }

    Status = NewStatus;
    MarkStateChanged();
    StatusChanged.Broadcast(Status);
}
