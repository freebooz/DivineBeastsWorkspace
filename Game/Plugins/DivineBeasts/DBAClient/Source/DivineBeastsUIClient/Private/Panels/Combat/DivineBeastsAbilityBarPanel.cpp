#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UDivineBeastsAbilityBarPanel::NativeConstruct()
{
    Super::NativeConstruct();
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.AddDynamic(
            this, &UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged);
    }
    RefreshAbilitySourceFromOwningPawn();
}

void UDivineBeastsAbilityBarPanel::NativeDestruct()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged);
    }
    if (AbilityBarViewModel)
    {
        AbilityBarViewModel->OnSlotsChanged().Remove(ViewModelSlotsHandle);
        AbilityBarViewModel->UnbindFromLoadout();
    }
    ViewModelSlotsHandle.Reset();
    Super::NativeDestruct();
}

void UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged(
    APawn* PreviousPawn, APawn* NewPawn)
{
    // 无论新旧角色是否同名都必须按当前 Controller 获取授权数据源。
    RefreshAbilitySourceFromOwningPawn();
}

bool UDivineBeastsAbilityBarPanel::RefreshAbilitySourceFromOwningPawn()
{
    if (!AbilityBarViewModel)
    {
        AbilityBarViewModel = NewObject<UDivineBeastsAbilityBarViewModel>(this);
        ViewModelSlotsHandle = AbilityBarViewModel->OnSlotsChanged().AddUObject(
            this, &UDivineBeastsAbilityBarPanel::HandleViewModelSlotsChanged);
    }
    APawn* Pawn = GetOwningPlayerPawn();
    UDivineBeastsAbilityLoadoutComponent* Loadout = Pawn
        ? Pawn->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>()
        : nullptr;
    const bool bBound = AbilityBarViewModel->BindToLoadout(Loadout);
    ApplyAbilitySlots(AbilityBarViewModel->GetSlotsRef());
    return bBound;
}

void UDivineBeastsAbilityBarPanel::HandleViewModelSlotsChanged(
    const TArray<FGamePlatformUISlotState>& NewSlots)
{
    ApplyAbilitySlots(NewSlots);
}


void UDivineBeastsAbilityBarPanel::ApplyAbilitySlots(
    const TArray<FGamePlatformUISlotState>& InSlots)
{
    bool bChanged = AbilitySlots.Num() != InSlots.Num();

    if (!bChanged)
    {
        for (int32 Index = 0; Index < InSlots.Num(); ++Index)
        {
            const FGamePlatformUISlotState& A = AbilitySlots[Index];
            const FGamePlatformUISlotState& B = InSlots[Index];

            if (A.SlotId != B.SlotId ||
                A.ContentId != B.ContentId ||
                A.Icon != B.Icon ||
                A.Count != B.Count ||
                !FMath::IsNearlyEqual(
                    A.OverlayProgress,
                    B.OverlayProgress,
                    0.0001f) ||
                A.bEnabled != B.bEnabled ||
                A.bPending != B.bPending)
            {
                bChanged = true;
                break;
            }
        }
    }

    if (!bChanged)
    {
        return;
    }

    AbilitySlots = InSlots;
    for (FGamePlatformUISlotState& AbilitySlotState : AbilitySlots)
    {
        AbilitySlotState.OverlayProgress =
            FMath::Clamp(
                AbilitySlotState.OverlayProgress,
                0.0f,
                1.0f);
    }

    NotifyCombatPresentationChanged();
    BP_OnAbilitySlotsChanged();
}
