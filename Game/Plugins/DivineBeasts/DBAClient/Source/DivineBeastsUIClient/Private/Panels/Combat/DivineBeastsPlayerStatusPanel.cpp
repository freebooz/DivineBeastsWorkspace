#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"

#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"

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

void UDivineBeastsPlayerStatusPanel::BindStatusViewModel(UDivineBeastsPlayerStatusViewModel* InViewModel)
{
    ClearStatusViewModel();
    if (!IsValid(InViewModel))
    {
        return;
    }

    StatusViewModel = InViewModel;
    StatusChangedHandle = StatusViewModel->OnStatusChanged().AddUObject(
        this, &UDivineBeastsPlayerStatusPanel::HandleStatusChanged);
    ApplyStatus(StatusViewModel->GetStatusRef());
}

void UDivineBeastsPlayerStatusPanel::ClearStatusViewModel()
{
    if (IsValid(StatusViewModel) && StatusChangedHandle.IsValid())
    {
        StatusViewModel->OnStatusChanged().Remove(StatusChangedHandle);
    }
    StatusChangedHandle.Reset();
    StatusViewModel = nullptr;
}

void UDivineBeastsPlayerStatusPanel::NativeConstruct()
{
    Super::NativeConstruct();
    if (IsValid(ShieldBar))
    {
        // Shield不是GAS属性，不得把历史蓝图中的空盾资源条显示给玩家。
        ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDivineBeastsPlayerStatusPanel::NativeDestruct()
{
    ClearStatusViewModel();
    Super::NativeDestruct();
}

void UDivineBeastsPlayerStatusPanel::HandleStatusChanged(
    const FDivineBeastsPlayerStatusViewData& NewStatus)
{
    ApplyStatus(NewStatus);
}

void UDivineBeastsPlayerStatusPanel::ApplyStatus(
    const FDivineBeastsPlayerStatusViewData& InStatus)
{
    const bool bEquivalent =
        FMath::IsNearlyEqual(Status.Health, InStatus.Health, 0.0001) &&
        FMath::IsNearlyEqual(Status.MaxHealth, InStatus.MaxHealth, 0.0001) &&
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
        ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (IsValid(MomentumBar))
    {
        MomentumBar->ApplyResourceState(
            MakeBarState(
                TEXT("Momentum"),
                Status.Momentum,
                Status.MaxMomentum));
    }

    NotifyCombatPresentationChanged();
    BP_OnPlayerStatusChanged(Status);
}
