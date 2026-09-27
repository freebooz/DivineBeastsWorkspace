#include "Services/GamePlatformInventoryClientSubsystem.h"

#include "Interfaces/GamePlatformInventoryClientTransport.h"

void UGamePlatformInventoryClientSubsystem::Deinitialize()
{
    OnChanged.Clear();
    ResetAccount();
    Super::Deinitialize();
}

bool UGamePlatformInventoryClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformInventoryClientTransport, ESPMode::ThreadSafe> InTransport)
{
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    SetState(EGamePlatformInventoryClientState::Loading, EGamePlatformInventoryError::None);
    return RefreshSnapshot();
}

void UGamePlatformInventoryClientSubsystem::ResetAccount()
{
    ++AccountGeneration;
    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Snapshot = {};
    Pending.Reset();
    ++SnapshotGeneration;
    CachedSortedItems.Reset();
    CachedItemIndexByInstanceId.Reset();
    CachedViewModels.Reset();
    CachedSnapshotGeneration = ~uint64(0);
    CachedViewSnapshotGeneration = ~uint64(0);
    CachedViewPendingOperationId.Invalidate();
    CachedViewPendingType = EGamePlatformInventoryOperationType::None;
    Transport.Reset();
    LastError = EGamePlatformInventoryError::None;
    State = EGamePlatformInventoryClientState::Uninitialized;
    OnChanged.Broadcast();
}

bool UGamePlatformInventoryClientSubsystem::RefreshSnapshot()
{
    if (!Transport.IsValid() || CurrentAccountKey.IsEmpty())
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InventoryNotLoaded);
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    if (State != EGamePlatformInventoryClientState::Reconciling)
    {
        SetState(
            EGamePlatformInventoryClientState::Loading,
            EGamePlatformInventoryError::None);
    }

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem> WeakThis(this);
    const bool bStarted =
        Transport->BeginGetSnapshot(
            [WeakThis, ExpectedGeneration](
                FGamePlatformInventorySnapshot NewSnapshot,
                EGamePlatformInventoryError Error)
            {
                if (UGamePlatformInventoryClientSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::BackendUnavailable);
    }

    return bStarted;
}

void UGamePlatformInventoryClientSubsystem::EnsureSnapshotDerivedCache() const
{
    if (CachedSnapshotGeneration == SnapshotGeneration)
    {
        return;
    }

    CachedSortedItems = Snapshot.Items;
    CachedSortedItems.Sort(
        [](const FGamePlatformInventoryItemInstance& A,
           const FGamePlatformInventoryItemInstance& B)
        {
            if (A.ContainerId != B.ContainerId)
            {
                return A.ContainerId.LexicalLess(B.ContainerId);
            }
            if (A.SlotIndex != B.SlotIndex)
            {
                return A.SlotIndex < B.SlotIndex;
            }
            return A.ItemDefinitionId.LexicalLess(B.ItemDefinitionId);
        });

    CachedItemIndexByInstanceId.Reset();
    CachedItemIndexByInstanceId.Reserve(Snapshot.Items.Num());
    for (int32 Index = 0; Index < Snapshot.Items.Num(); ++Index)
    {
        const FString& ItemInstanceId = Snapshot.Items[Index].ItemInstanceId;
        if (!ItemInstanceId.IsEmpty())
        {
            CachedItemIndexByInstanceId.Add(ItemInstanceId, Index);
        }
    }

    CachedSnapshotGeneration = SnapshotGeneration;
}

void UGamePlatformInventoryClientSubsystem::EnsureViewModelCache() const
{
    EnsureSnapshotDerivedCache();
    if (CachedViewSnapshotGeneration == SnapshotGeneration &&
        CachedViewPendingOperationId == Pending.OperationId &&
        CachedViewPendingType == Pending.Type)
    {
        return;
    }

    CachedViewModels.Reset();
    CachedViewModels.Reserve(CachedSortedItems.Num());
    for (const FGamePlatformInventoryItemInstance& Item : CachedSortedItems)
    {
        FGamePlatformInventoryItemViewModel View;
        View.ItemInstanceId = Item.ItemInstanceId;
        View.ItemDefinitionId = Item.ItemDefinitionId;
        View.Quantity = Item.Quantity;
        View.ContainerId = Item.ContainerId;
        View.SlotIndex = Item.SlotIndex;
        View.DisplayDefinitionHandle = Item.ItemDefinitionId;

        switch (Pending.Type)
        {
        case EGamePlatformInventoryOperationType::Move:
            View.bPending = Pending.Move.ItemInstanceId == Item.ItemInstanceId;
            break;
        case EGamePlatformInventoryOperationType::Split:
            View.bPending = Pending.Split.SourceItemInstanceId == Item.ItemInstanceId;
            break;
        case EGamePlatformInventoryOperationType::Merge:
            View.bPending =
                Pending.Merge.SourceItemInstanceId == Item.ItemInstanceId ||
                Pending.Merge.TargetItemInstanceId == Item.ItemInstanceId;
            break;
        case EGamePlatformInventoryOperationType::SetQuickbar:
            View.bPending = Pending.Quickbar.ItemInstanceId == Item.ItemInstanceId;
            break;
        default:
            break;
        }

        CachedViewModels.Add(MoveTemp(View));
    }

    CachedViewSnapshotGeneration = SnapshotGeneration;
    CachedViewPendingOperationId = Pending.OperationId;
    CachedViewPendingType = Pending.Type;
}

const TArray<FGamePlatformInventoryItemInstance>&
UGamePlatformInventoryClientSubsystem::GetSortedItemsView() const
{
    EnsureSnapshotDerivedCache();
    return CachedSortedItems;
}

TArray<FGamePlatformInventoryItemInstance>
UGamePlatformInventoryClientSubsystem::GetSortedItems() const
{
    return GetSortedItemsView();
}

const TArray<FGamePlatformInventoryItemViewModel>&
UGamePlatformInventoryClientSubsystem::GetViewModelsView() const
{
    EnsureViewModelCache();
    return CachedViewModels;
}

TArray<FGamePlatformInventoryItemViewModel>
UGamePlatformInventoryClientSubsystem::GetViewModels() const
{
    return GetViewModelsView();
}

const FGamePlatformInventoryItemInstance*
UGamePlatformInventoryClientSubsystem::FindItem(
    const FString& ItemInstanceId) const
{
    if (ItemInstanceId.IsEmpty())
    {
        return nullptr;
    }

    EnsureSnapshotDerivedCache();
    const int32* Index = CachedItemIndexByInstanceId.Find(ItemInstanceId);
    return Index && Snapshot.Items.IsValidIndex(*Index)
        ? &Snapshot.Items[*Index]
        : nullptr;
}

FGuid UGamePlatformInventoryClientSubsystem::RequestMove(
    const FString& ItemInstanceId,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    if (!BeginPendingOperation() ||
        ItemInstanceId.IsEmpty() ||
        TargetContainerId.IsNone() ||
        TargetSlotIndex < 0)
    {
        return {};
    }

    Pending.Type = EGamePlatformInventoryOperationType::Move;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Move.OperationId = Pending.OperationId;
    Pending.Move.ExpectedRevision = Snapshot.InventoryRevision;
    Pending.Move.ItemInstanceId = ItemInstanceId;
    Pending.Move.TargetContainerId = TargetContainerId;
    Pending.Move.TargetSlotIndex = TargetSlotIndex;

    return SendPendingOperation() ? Pending.OperationId : FGuid();
}

FGuid UGamePlatformInventoryClientSubsystem::RequestSplit(
    const FString& SourceItemInstanceId,
    int32 SplitQuantity,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    if (!BeginPendingOperation() ||
        SourceItemInstanceId.IsEmpty() ||
        SplitQuantity <= 0 ||
        TargetContainerId.IsNone() ||
        TargetSlotIndex < 0)
    {
        return {};
    }

    Pending.Type = EGamePlatformInventoryOperationType::Split;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Split.OperationId = Pending.OperationId;
    Pending.Split.ExpectedRevision = Snapshot.InventoryRevision;
    Pending.Split.SourceItemInstanceId = SourceItemInstanceId;
    Pending.Split.SplitQuantity = SplitQuantity;
    Pending.Split.TargetContainerId = TargetContainerId;
    Pending.Split.TargetSlotIndex = TargetSlotIndex;

    return SendPendingOperation() ? Pending.OperationId : FGuid();
}

FGuid UGamePlatformInventoryClientSubsystem::RequestMerge(
    const FString& SourceItemInstanceId,
    const FString& TargetItemInstanceId)
{
    if (!BeginPendingOperation() ||
        SourceItemInstanceId.IsEmpty() ||
        TargetItemInstanceId.IsEmpty() ||
        SourceItemInstanceId == TargetItemInstanceId)
    {
        return {};
    }

    Pending.Type = EGamePlatformInventoryOperationType::Merge;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Merge.OperationId = Pending.OperationId;
    Pending.Merge.ExpectedRevision = Snapshot.InventoryRevision;
    Pending.Merge.SourceItemInstanceId = SourceItemInstanceId;
    Pending.Merge.TargetItemInstanceId = TargetItemInstanceId;

    return SendPendingOperation() ? Pending.OperationId : FGuid();
}

FGuid UGamePlatformInventoryClientSubsystem::RequestSetQuickbar(
    int32 SlotIndex,
    const FString& ItemInstanceId)
{
    if (!BeginPendingOperation() ||
        SlotIndex < 0 ||
        ItemInstanceId.IsEmpty())
    {
        return {};
    }

    Pending.Type = EGamePlatformInventoryOperationType::SetQuickbar;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Quickbar.OperationId = Pending.OperationId;
    Pending.Quickbar.ExpectedRevision = Snapshot.InventoryRevision;
    Pending.Quickbar.SlotIndex = SlotIndex;
    Pending.Quickbar.ItemInstanceId = ItemInstanceId;

    return SendPendingOperation() ? Pending.OperationId : FGuid();
}

FGuid UGamePlatformInventoryClientSubsystem::RequestClearQuickbar(
    int32 SlotIndex)
{
    if (!BeginPendingOperation() || SlotIndex < 0)
    {
        return {};
    }

    Pending.Type = EGamePlatformInventoryOperationType::ClearQuickbar;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Quickbar.OperationId = Pending.OperationId;
    Pending.Quickbar.ExpectedRevision = Snapshot.InventoryRevision;
    Pending.Quickbar.SlotIndex = SlotIndex;

    return SendPendingOperation() ? Pending.OperationId : FGuid();
}

bool UGamePlatformInventoryClientSubsystem::BeginPendingOperation()
{
    return State == EGamePlatformInventoryClientState::Ready &&
           Snapshot.InventoryRevision > 0 &&
           Transport.IsValid() &&
           Pending.Type == EGamePlatformInventoryOperationType::None;
}

bool UGamePlatformInventoryClientSubsystem::SendPendingOperation()
{
    if (!Transport.IsValid() ||
        Pending.Type == EGamePlatformInventoryOperationType::None ||
        !Pending.OperationId.IsValid())
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const FGuid ExpectedOperationId = Pending.OperationId;
    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem> WeakThis(this);

    FGamePlatformInventoryMutationCompletion Completion =
        [WeakThis, ExpectedGeneration, ExpectedOperationId](
            FGamePlatformInventoryMutationResult Result,
            EGamePlatformInventoryError Error)
        {
            if (UGamePlatformInventoryClientSubsystem* Self = WeakThis.Get())
            {
                Self->HandleMutationCompleted(
                    ExpectedGeneration,
                    ExpectedOperationId,
                    MoveTemp(Result),
                    Error);
            }
        };

    bool bStarted = false;
    switch (Pending.Type)
    {
    case EGamePlatformInventoryOperationType::Move:
        bStarted = Transport->BeginMove(Pending.Move, MoveTemp(Completion));
        break;
    case EGamePlatformInventoryOperationType::Split:
        bStarted = Transport->BeginSplit(Pending.Split, MoveTemp(Completion));
        break;
    case EGamePlatformInventoryOperationType::Merge:
        bStarted = Transport->BeginMerge(Pending.Merge, MoveTemp(Completion));
        break;
    case EGamePlatformInventoryOperationType::SetQuickbar:
        bStarted = Transport->BeginSetQuickbar(Pending.Quickbar, MoveTemp(Completion));
        break;
    case EGamePlatformInventoryOperationType::ClearQuickbar:
        bStarted = Transport->BeginClearQuickbar(Pending.Quickbar, MoveTemp(Completion));
        break;
    default:
        break;
    }

    if (bStarted)
    {
        SetState(
            EGamePlatformInventoryClientState::Mutating,
            EGamePlatformInventoryError::None);
        return true;
    }

    SetState(
        EGamePlatformInventoryClientState::Error,
        EGamePlatformInventoryError::BackendUnavailable);
    return false;
}

bool UGamePlatformInventoryClientSubsystem::RetryPendingOperation()
{
    if (State != EGamePlatformInventoryClientState::Error ||
        Pending.Type == EGamePlatformInventoryOperationType::None ||
        !Pending.OperationId.IsValid() ||
        !Transport.IsValid())
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const FGuid ExpectedOperationId = Pending.OperationId;
    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetOperation(
            Pending.OperationId,
            [WeakThis, ExpectedGeneration, ExpectedOperationId](
                FGamePlatformInventoryMutationResult Result,
                EGamePlatformInventoryError Error)
            {
                if (UGamePlatformInventoryClientSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleOperationQueryCompleted(
                        ExpectedGeneration,
                        ExpectedOperationId,
                        MoveTemp(Result),
                        Error);
                }
            });

    if (bStarted)
    {
        SetState(
            EGamePlatformInventoryClientState::Reconciling,
            EGamePlatformInventoryError::None);
    }

    return bStarted;
}

void UGamePlatformInventoryClientSubsystem::HandleOperationQueryCompleted(
    uint64 ExpectedGeneration,
    FGuid ExpectedOperationId,
    FGamePlatformInventoryMutationResult Result,
    EGamePlatformInventoryError Error)
{
    if (ExpectedGeneration != AccountGeneration ||
        Pending.OperationId != ExpectedOperationId)
    {
        return;
    }

    if (Error == EGamePlatformInventoryError::OperationNotFound)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::OutcomeUnknown);

        // 仅在后端明确确认OperationId尚未落库后，才复用同一个OperationId重发一次。
        SendPendingOperation();
        return;
    }

    if (Error != EGamePlatformInventoryError::None)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            Error);
        return;
    }

    if (!Result.OperationId.IsValid() ||
        Result.OperationId != ExpectedOperationId ||
        !ApplySnapshot(Result.Snapshot))
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    Pending.Reset();
    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

void UGamePlatformInventoryClientSubsystem::HandleSnapshotCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformInventorySnapshot NewSnapshot,
    EGamePlatformInventoryError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    if (Error != EGamePlatformInventoryError::None)
    {
        SetState(EGamePlatformInventoryClientState::Error, Error);
        return;
    }

    if (!ApplySnapshot(NewSnapshot))
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    if (Pending.Type != EGamePlatformInventoryOperationType::None &&
        State == EGamePlatformInventoryClientState::Reconciling)
    {
        Pending.Reset();
    }

    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

void UGamePlatformInventoryClientSubsystem::HandleMutationCompleted(
    uint64 ExpectedGeneration,
    FGuid ExpectedOperationId,
    FGamePlatformInventoryMutationResult Result,
    EGamePlatformInventoryError Error)
{
    if (ExpectedGeneration != AccountGeneration ||
        Pending.OperationId != ExpectedOperationId)
    {
        return;
    }

    if (Error == EGamePlatformInventoryError::RevisionConflict)
    {
        SetState(
            EGamePlatformInventoryClientState::Reconciling,
            Error);
        RefreshSnapshot();
        return;
    }

    if (Error != EGamePlatformInventoryError::None)
    {
        SetState(EGamePlatformInventoryClientState::Error, Error);
        return;
    }

    if (!Result.OperationId.IsValid() ||
        Result.OperationId != ExpectedOperationId ||
        !ApplySnapshot(Result.Snapshot))
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    Pending.Reset();
    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

bool UGamePlatformInventoryClientSubsystem::ApplySnapshot(
    const FGamePlatformInventorySnapshot& NewSnapshot)
{
    if (NewSnapshot.InventoryRevision < Snapshot.InventoryRevision ||
        NewSnapshot.InventoryRevision <= 0)
    {
        return false;
    }

    for (const FGamePlatformInventoryItemInstance& Item :
         NewSnapshot.Items)
    {
        if (!Item.IsValid())
        {
            return false;
        }
    }

    Snapshot = NewSnapshot;
    ++SnapshotGeneration;
    return true;
}

void UGamePlatformInventoryClientSubsystem::SetState(
    EGamePlatformInventoryClientState NewState,
    EGamePlatformInventoryError Error)
{
    State = NewState;
    LastError = Error;
    OnChanged.Broadcast();
}
