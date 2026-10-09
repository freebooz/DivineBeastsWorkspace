#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"

#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(
            this, &UDivineBeastsPlayerStatusPanel::HandlePossessedPawnChanged);
    }
    if (IsValid(ShieldBar))
    {
        // ShieldBar只是旧控件蓝图兼容字段，不再绑定或展示任何盾容量数值。
        ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshStatusSourceFromOwningPawn();
}

void UDivineBeastsPlayerStatusPanel::NativeDestruct()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsPlayerStatusPanel::HandlePossessedPawnChanged);
    }
    if (IsValid(OwnedStatusViewModel))
    {
        OwnedStatusViewModel->UnbindFromAbilitySystem();
    }
    ClearStatusViewModel();
    OwnedStatusViewModel = nullptr;
    Super::NativeDestruct();
}

void UDivineBeastsPlayerStatusPanel::HandlePossessedPawnChanged(
    APawn* /*PreviousPawn*/, APawn* /*NewPawn*/)
{
    RefreshStatusSourceFromOwningPawn();
}

void UDivineBeastsPlayerStatusPanel::RefreshStatusSourceFromOwningPawn()
{
    if (!IsValid(OwnedStatusViewModel))
    {
        OwnedStatusViewModel = NewObject<UDivineBeastsPlayerStatusViewModel>(this);
    }

    const APawn* Pawn = GetOwningPlayerPawn();
    UGamePlatformAbilitySystemComponent* ASC = IsValid(Pawn)
        ? Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>()
        : nullptr;

    if (!IsValid(ASC))
    {
        // Pawn切换/失效必须清空旧玩家的生命与气势，不能显示残留快照。
        OwnedStatusViewModel->UnbindFromAbilitySystem();
        ClearStatusViewModel();
        ApplyStatus(FDivineBeastsPlayerStatusViewData());
        return;
    }

    // UI ViewModel只订阅现有ASC的复制属性：Health/MaxHealth/Momentum/MaxMomentum。
    // 自己不进行GameplayEffect写入、技能授权或每帧轮询。
    if (OwnedStatusViewModel->BindToAbilitySystem(ASC))
    {
        BindStatusViewModel(OwnedStatusViewModel);
    }
    else
    {
        ClearStatusViewModel();
        ApplyStatus(FDivineBeastsPlayerStatusViewData());
    }
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
