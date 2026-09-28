#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"

namespace
{
FGamePlatformUIResourceBarState MakeBarState(
    FName ResourceId,
    double Current,
    double Maximum)
{
    FGamePlatformUIResourceBarState State;
    State.ResourceId = ResourceId;
    State.CurrentValue = Current;
    State.MaximumValue = Maximum;
    return State;
}
}

void UDivineBeastsPlayerStatusPanel::ApplyStatus(
    const FDivineBeastsPlayerStatusViewData& InStatus)
{
    const bool bEquivalent =
        FMath::IsNearlyEqual(Status.Health, InStatus.Health, 0.0001) &&
        FMath::IsNearlyEqual(Status.MaxHealth, InStatus.MaxHealth, 0.0001) &&
        FMath::IsNearlyEqual(Status.Shield, InStatus.Shield, 0.0001) &&
        FMath::IsNearlyEqual(Status.MaxShield, InStatus.MaxShield, 0.0001) &&
        FMath::IsNearlyEqual(Status.Momentum, InStatus.Momentum, 0.0001) &&
        FMath::IsNearlyEqual(Status.MaxMomentum, InStatus.MaxMomentum, 0.0001) &&
        Status.bDead == InStatus.bDead;

    if (bEquivalent)
    {
        return;
    }

    Status = InStatus;

    if (IsValid(HealthBar))
    {
        HealthBar->ApplyResourceState(
            MakeBarState(
                TEXT("Health"),
                Status.Health,
                Status.MaxHealth));
    }
    if (IsValid(ShieldBar))
    {
        ShieldBar->ApplyResourceState(
            MakeBarState(
                TEXT("Shield"),
                Status.Shield,
                Status.MaxShield));
    }
    if (IsValid(MomentumBar))
    {
        MomentumBar->ApplyResourceState(
            MakeBarState(
                TEXT("Momentum"),
                Status.Momentum,
                Status.MaxMomentum));
    }

    BP_OnPlayerStatusChanged(Status);
}
