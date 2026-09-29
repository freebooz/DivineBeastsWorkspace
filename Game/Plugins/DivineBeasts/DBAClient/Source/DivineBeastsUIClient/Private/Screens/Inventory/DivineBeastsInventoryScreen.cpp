#include "Screens/Inventory/DivineBeastsInventoryScreen.h"

#include "Engine/LocalPlayer.h"

namespace
{
const TArray<FGamePlatformInventoryItemViewModel>& EmptyInventoryItems()
{
    static const TArray<FGamePlatformInventoryItemViewModel> Empty;
    return Empty;
}
}

EGamePlatformInventoryClientState
UDivineBeastsInventoryScreen::GetInventoryState() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetState()
        : EGamePlatformInventoryClientState::Uninitialized;
}

int64 UDivineBeastsInventoryScreen::GetInventoryRevision() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetInventoryRevision()
        : 0;
}

bool UDivineBeastsInventoryScreen::HasPendingInventoryOperation() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem) &&
        Subsystem->HasPendingOperation();
}

TArray<FGamePlatformInventoryItemViewModel>
UDivineBeastsInventoryScreen::GetInventoryItems() const
{
    return GetInventoryItemsView();
}

const TArray<FGamePlatformInventoryItemViewModel>&
UDivineBeastsInventoryScreen::GetInventoryItemsView() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetViewModelsView()
        : EmptyInventoryItems();
}

bool UDivineBeastsInventoryScreen::RefreshInventory()
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem) &&
        Subsystem->RefreshSnapshot();
}

FGuid UDivineBeastsInventoryScreen::RequestMoveItem(
    const FString& ItemInstanceId,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestMove(
            ItemInstanceId,
            TargetContainerId,
            TargetSlotIndex)
        : FGuid();
}

FGuid UDivineBeastsInventoryScreen::RequestSplitItem(
    const FString& SourceItemInstanceId,
    int32 SplitQuantity,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestSplit(
            SourceItemInstanceId,
            SplitQuantity,
            TargetContainerId,
            TargetSlotIndex)
        : FGuid();
}

FGuid UDivineBeastsInventoryScreen::RequestMergeItem(
    const FString& SourceItemInstanceId,
    const FString& TargetItemInstanceId)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestMerge(
            SourceItemInstanceId,
            TargetItemInstanceId)
        : FGuid();
}

void UDivineBeastsInventoryScreen::BindUIEvents()
{
    Super::BindUIEvents();

    InventorySubsystem = ResolveInventorySubsystem();
    if (IsValid(InventorySubsystem) &&
        !InventoryChangedHandle.IsValid())
    {
        InventoryChangedHandle =
            InventorySubsystem->OnChanged.AddUObject(
                this,
                &UDivineBeastsInventoryScreen::HandleInventoryChanged);
    }
}

void UDivineBeastsInventoryScreen::UnbindUIEvents()
{
    if (IsValid(InventorySubsystem) &&
        InventoryChangedHandle.IsValid())
    {
        InventorySubsystem->OnChanged.Remove(InventoryChangedHandle);
        InventoryChangedHandle.Reset();
    }
    InventorySubsystem = nullptr;

    Super::UnbindUIEvents();
}

void UDivineBeastsInventoryScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    HandleInventoryChanged();
}

void UDivineBeastsInventoryScreen::HandleInventoryChanged()
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    if (!IsValid(Subsystem))
    {
        BP_OnInventoryViewChanged(
            0,
            EGamePlatformInventoryClientState::Uninitialized,
            false);
        return;
    }

    BP_OnInventoryViewChanged(
        Subsystem->GetInventoryRevision(),
        Subsystem->GetState(),
        Subsystem->HasPendingOperation());
}

UGamePlatformInventoryClientSubsystem*
UDivineBeastsInventoryScreen::ResolveInventorySubsystem() const
{
    if (IsValid(InventorySubsystem))
    {
        return InventorySubsystem;
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    return IsValid(LocalPlayer)
        ? LocalPlayer->GetSubsystem<UGamePlatformInventoryClientSubsystem>()
        : nullptr;
}
