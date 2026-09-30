#include "Services/GamePlatformInventoryClientSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Interfaces/GamePlatformInventoryClientTransport.h"
#include "Transport/GamePlatformInventoryGatewayHttpTransport.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformInventory, Log, All);

namespace
{
constexpr int32 MaxInventoryContainers = 64;
constexpr int32 MaxInventoryItems = 10000;
constexpr int32 MaxInventoryQuickbarSlots = 12;

struct FInventorySlotKey
{
    FName ContainerId = NAME_None;
    int32 SlotIndex = INDEX_NONE;

    bool operator==(const FInventorySlotKey& Other) const
    {
        return ContainerId == Other.ContainerId &&
               SlotIndex == Other.SlotIndex;
    }
};

uint32 GetTypeHash(const FInventorySlotKey& Key)
{
    // FName哈希通过ADL解析；SlotIndex直接转uint32，避免当前自定义GetTypeHash重载遮蔽标量重载。
    return HashCombine(
        GetTypeHash(Key.ContainerId),
        static_cast<uint32>(Key.SlotIndex));
}
}

void UGamePlatformInventoryClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BindOnlineAuthentication();
}

void UGamePlatformInventoryClientSubsystem::Deinitialize()
{
    UnbindOnlineAuthentication();
    OnChanged.Clear();
    ResetAccount();
    Super::Deinitialize();
}

void UGamePlatformInventoryClientSubsystem::BindOnlineAuthentication()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UGameInstance* GameInstance =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetGameInstance()
            : nullptr;

    UGamePlatformOnlineClientSubsystem* Online =
        IsValid(GameInstance)
            ? GameInstance->GetSubsystem<
                  UGamePlatformOnlineClientSubsystem>()
            : nullptr;

    if (!IsValid(Online))
    {
        return;
    }

    OnlineSubsystem = Online;
    if (!AuthStateChangedHandle.IsValid())
    {
        AuthStateChangedHandle =
            Online->OnAuthStateChanged().AddUObject(
                this,
                &UGamePlatformInventoryClientSubsystem::
                    HandleAuthStateChanged);
    }

    HandleAuthStateChanged(Online->GetSnapshot());
}

void UGamePlatformInventoryClientSubsystem::UnbindOnlineAuthentication()
{
    if (UGamePlatformOnlineClientSubsystem* Online =
            OnlineSubsystem.Get())
    {
        if (AuthStateChangedHandle.IsValid())
        {
            Online->OnAuthStateChanged().Remove(
                AuthStateChangedHandle);
        }
    }

    AuthStateChangedHandle.Reset();
    OnlineSubsystem.Reset();
}

void UGamePlatformInventoryClientSubsystem::HandleAuthStateChanged(
    const FGamePlatformAuthSnapshot& AuthSnapshot)
{
    switch (AuthSnapshot.State)
    {
    case EGamePlatformAuthState::Authenticated:
    {
        UGamePlatformOnlineClientSubsystem* Online =
            OnlineSubsystem.Get();
        if (!IsValid(Online) ||
            AuthSnapshot.AccountId.IsEmpty())
        {
            ResetAccount();
            SetState(
                EGamePlatformInventoryClientState::Error,
                EGamePlatformInventoryError::Unauthorized);
            return;
        }

        if (CurrentAccountKey == AuthSnapshot.AccountId &&
            Transport.IsValid())
        {
            return;
        }

        TSharedPtr<
            IGamePlatformInventoryClientTransport,
            ESPMode::ThreadSafe> NewTransport =
                MakeShared<
                    FGamePlatformInventoryGatewayHttpTransport,
                    ESPMode::ThreadSafe>(Online);

        if (!ConfigureAuthenticatedAccount(
                AuthSnapshot.AccountId,
                MoveTemp(NewTransport)))
        {
            SetState(
                EGamePlatformInventoryClientState::Error,
                EGamePlatformInventoryError::BackendUnavailable);
        }
        return;
    }

    case EGamePlatformAuthState::Refreshing:
        // Online负责single-flight刷新；背包保留同一账号快照和Pending Operation。
        return;

    case EGamePlatformAuthState::LoggedOut:
    case EGamePlatformAuthState::LoggingIn:
    case EGamePlatformAuthState::LoggingOut:
    case EGamePlatformAuthState::Failed:
        if (!CurrentAccountKey.IsEmpty() ||
            Transport.IsValid() ||
            State != EGamePlatformInventoryClientState::Uninitialized)
        {
            ResetAccount();
        }
        return;

    default:
        return;
    }
}

bool UGamePlatformInventoryClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<
        IGamePlatformInventoryClientTransport,
        ESPMode::ThreadSafe> InTransport)
{
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    SetState(
        EGamePlatformInventoryClientState::Loading,
        EGamePlatformInventoryError::None);
    return RefreshSnapshot();
}

void UGamePlatformInventoryClientSubsystem::ResetAccount()
{
    ++AccountGeneration;
    ++SnapshotRequestGeneration;

    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Snapshot = {};
    Pending.Reset();

    ++SnapshotGeneration;
    bSnapshotRequestInFlight = false;
    bConflictSnapshotReconcile = false;

    CachedSortedItems.Reset();
    CachedItemIndexByInstanceId.Reset();
    CachedViewModels.Reset();
    CachedSnapshotGeneration = ~uint64(0);
    CachedViewSnapshotGeneration = ~uint64(0);
    CachedViewPendingOperationId.Invalidate();
    CachedViewPendingType =
        EGamePlatformInventoryOperationType::None;

    Transport.Reset();
    LastError = EGamePlatformInventoryError::None;
    State = EGamePlatformInventoryClientState::Uninitialized;
    OnChanged.Broadcast();
}

bool UGamePlatformInventoryClientSubsystem::RefreshSnapshot()
{
    if (!Transport.IsValid() ||
        CurrentAccountKey.IsEmpty())
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InventoryNotLoaded);
        return false;
    }

    const bool bHasPending =
        Pending.Type != EGamePlatformInventoryOperationType::None;
    const bool bConflictReconcile =
        bConflictSnapshotReconcile &&
        bHasPending &&
        (State == EGamePlatformInventoryClientState::Reconciling ||
         State == EGamePlatformInventoryClientState::Error);

    if (bSnapshotRequestInFlight ||
        State == EGamePlatformInventoryClientState::Mutating ||
        (bHasPending && !bConflictReconcile))
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const uint64 ExpectedSnapshotRequestGeneration =
        ++SnapshotRequestGeneration;

    bSnapshotRequestInFlight = true;

    SetState(
        bConflictReconcile
            ? EGamePlatformInventoryClientState::Reconciling
            : EGamePlatformInventoryClientState::Loading,
        bConflictReconcile
            ? EGamePlatformInventoryError::RevisionConflict
            : EGamePlatformInventoryError::None);

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    const bool bStarted =
        Transport->BeginGetSnapshot(
            [WeakThis,
             ExpectedGeneration,
             ExpectedSnapshotRequestGeneration,
             bConflictReconcile](
                FGamePlatformInventorySnapshot NewSnapshot,
                EGamePlatformInventoryError Error)
            {
                if (UGamePlatformInventoryClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        ExpectedSnapshotRequestGeneration,
                        bConflictReconcile,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted &&
        ExpectedGeneration == AccountGeneration &&
        ExpectedSnapshotRequestGeneration ==
            SnapshotRequestGeneration)
    {
        bSnapshotRequestInFlight = false;
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::BackendUnavailable);
    }

    return bStarted;
}

void UGamePlatformInventoryClientSubsystem::
EnsureSnapshotDerivedCache() const
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
            return A.ItemDefinitionId.LexicalLess(
                B.ItemDefinitionId);
        });

    CachedItemIndexByInstanceId.Reset();
    CachedItemIndexByInstanceId.Reserve(
        Snapshot.Items.Num());

    for (int32 Index = 0;
         Index < Snapshot.Items.Num();
         ++Index)
    {
        const FString& ItemInstanceId =
            Snapshot.Items[Index].ItemInstanceId;
        if (!ItemInstanceId.IsEmpty())
        {
            CachedItemIndexByInstanceId.Add(
                ItemInstanceId,
                Index);
        }
    }

    CachedSnapshotGeneration = SnapshotGeneration;
}

void UGamePlatformInventoryClientSubsystem::
EnsureViewModelCache() const
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

    for (const FGamePlatformInventoryItemInstance& Item :
         CachedSortedItems)
    {
        FGamePlatformInventoryItemViewModel View;
        View.ItemInstanceId = Item.ItemInstanceId;
        View.ItemDefinitionId = Item.ItemDefinitionId;
        View.Quantity = Item.Quantity;
        View.MaxStackSize = Item.MaxStackSize;
        View.ContainerId = Item.ContainerId;
        View.SlotIndex = Item.SlotIndex;
        View.DisplayDefinitionHandle = Item.ItemDefinitionId;

        switch (Pending.Type)
        {
        case EGamePlatformInventoryOperationType::Move:
            View.bPending =
                Pending.Move.ItemInstanceId ==
                Item.ItemInstanceId;
            break;

        case EGamePlatformInventoryOperationType::Split:
            View.bPending =
                Pending.Split.SourceItemInstanceId ==
                Item.ItemInstanceId;
            break;

        case EGamePlatformInventoryOperationType::Merge:
            View.bPending =
                Pending.Merge.SourceItemInstanceId ==
                    Item.ItemInstanceId ||
                Pending.Merge.TargetItemInstanceId ==
                    Item.ItemInstanceId;
            break;

        case EGamePlatformInventoryOperationType::SetQuickbar:
            View.bPending =
                Pending.Quickbar.ItemInstanceId ==
                Item.ItemInstanceId;
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
    const int32* Index =
        CachedItemIndexByInstanceId.Find(ItemInstanceId);

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

    Pending.Type =
        EGamePlatformInventoryOperationType::Move;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Move.OperationId = Pending.OperationId;
    Pending.Move.ExpectedRevision =
        Snapshot.InventoryRevision;
    Pending.Move.ItemInstanceId = ItemInstanceId;
    Pending.Move.TargetContainerId = TargetContainerId;
    Pending.Move.TargetSlotIndex = TargetSlotIndex;

    const FGuid AcceptedOperationId = Pending.OperationId;
    SendPendingOperation();
    return AcceptedOperationId;
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

    Pending.Type =
        EGamePlatformInventoryOperationType::Split;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Split.OperationId = Pending.OperationId;
    Pending.Split.ExpectedRevision =
        Snapshot.InventoryRevision;
    Pending.Split.SourceItemInstanceId =
        SourceItemInstanceId;
    Pending.Split.SplitQuantity = SplitQuantity;
    Pending.Split.TargetContainerId =
        TargetContainerId;
    Pending.Split.TargetSlotIndex =
        TargetSlotIndex;

    const FGuid AcceptedOperationId = Pending.OperationId;
    SendPendingOperation();
    return AcceptedOperationId;
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

    Pending.Type =
        EGamePlatformInventoryOperationType::Merge;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Merge.OperationId = Pending.OperationId;
    Pending.Merge.ExpectedRevision =
        Snapshot.InventoryRevision;
    Pending.Merge.SourceItemInstanceId =
        SourceItemInstanceId;
    Pending.Merge.TargetItemInstanceId =
        TargetItemInstanceId;

    const FGuid AcceptedOperationId = Pending.OperationId;
    SendPendingOperation();
    return AcceptedOperationId;
}

FGuid UGamePlatformInventoryClientSubsystem::RequestSetQuickbar(
    int32 SlotIndex,
    const FString& ItemInstanceId)
{
    if (!BeginPendingOperation() ||
        SlotIndex < 0 ||
        SlotIndex >= MaxInventoryQuickbarSlots ||
        ItemInstanceId.IsEmpty())
    {
        return {};
    }

    Pending.Type =
        EGamePlatformInventoryOperationType::SetQuickbar;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Quickbar.OperationId = Pending.OperationId;
    Pending.Quickbar.ExpectedRevision =
        Snapshot.InventoryRevision;
    Pending.Quickbar.SlotIndex = SlotIndex;
    Pending.Quickbar.ItemInstanceId = ItemInstanceId;

    const FGuid AcceptedOperationId = Pending.OperationId;
    SendPendingOperation();
    return AcceptedOperationId;
}

FGuid UGamePlatformInventoryClientSubsystem::RequestClearQuickbar(
    int32 SlotIndex)
{
    if (!BeginPendingOperation() ||
        SlotIndex < 0 ||
        SlotIndex >= MaxInventoryQuickbarSlots)
    {
        return {};
    }

    Pending.Type =
        EGamePlatformInventoryOperationType::ClearQuickbar;
    Pending.OperationId = FGuid::NewGuid();
    Pending.Quickbar.OperationId = Pending.OperationId;
    Pending.Quickbar.ExpectedRevision =
        Snapshot.InventoryRevision;
    Pending.Quickbar.SlotIndex = SlotIndex;

    const FGuid AcceptedOperationId = Pending.OperationId;
    SendPendingOperation();
    return AcceptedOperationId;
}

bool UGamePlatformInventoryClientSubsystem::
BeginPendingOperation()
{
    return State == EGamePlatformInventoryClientState::Ready &&
           Snapshot.InventoryRevision > 0 &&
           Transport.IsValid() &&
           !bSnapshotRequestInFlight &&
           Pending.Type ==
               EGamePlatformInventoryOperationType::None;
}

bool UGamePlatformInventoryClientSubsystem::
SendPendingOperation()
{
    if (!Transport.IsValid() ||
        Pending.Type ==
            EGamePlatformInventoryOperationType::None ||
        !Pending.OperationId.IsValid())
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const FGuid ExpectedOperationId = Pending.OperationId;

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    FGamePlatformInventoryMutationCompletion Completion =
        [WeakThis,
         ExpectedGeneration,
         ExpectedOperationId](
            FGamePlatformInventoryMutationResult Result,
            EGamePlatformInventoryError Error)
        {
            if (UGamePlatformInventoryClientSubsystem* Self =
                    WeakThis.Get())
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
        bStarted =
            Transport->BeginMove(
                Pending.Move,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::Split:
        bStarted =
            Transport->BeginSplit(
                Pending.Split,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::Merge:
        bStarted =
            Transport->BeginMerge(
                Pending.Merge,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::SetQuickbar:
        bStarted =
            Transport->BeginSetQuickbar(
                Pending.Quickbar,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::ClearQuickbar:
        bStarted =
            Transport->BeginClearQuickbar(
                Pending.Quickbar,
                MoveTemp(Completion));
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

bool UGamePlatformInventoryClientSubsystem::
RetryPendingOperation()
{
    if (Pending.Type ==
            EGamePlatformInventoryOperationType::None ||
        !Pending.OperationId.IsValid() ||
        !Transport.IsValid())
    {
        return false;
    }

    if (bConflictSnapshotReconcile)
    {
        return RefreshSnapshot();
    }

    if (State != EGamePlatformInventoryClientState::Error ||
        bSnapshotRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const FGuid ExpectedOperationId = Pending.OperationId;

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    const bool bStarted =
        Transport->BeginGetOperation(
            Pending.OperationId,
            [WeakThis,
             ExpectedGeneration,
             ExpectedOperationId](
                FGamePlatformInventoryMutationResult Result,
                EGamePlatformInventoryError Error)
            {
                if (UGamePlatformInventoryClientSubsystem* Self =
                        WeakThis.Get())
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

void UGamePlatformInventoryClientSubsystem::
HandleOperationQueryCompleted(
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

    if (Error ==
        EGamePlatformInventoryError::OperationNotFound)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::OutcomeUnknown);

        // 后端明确确认 OperationId 尚无持久结果后，才允许复用同一 OperationId 重发一次。
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
        !ApplySnapshot(MoveTemp(Result.Snapshot)))
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    Pending.Reset();
    bConflictSnapshotReconcile = false;
    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

void UGamePlatformInventoryClientSubsystem::
HandleSnapshotCompleted(
    uint64 ExpectedGeneration,
    uint64 ExpectedSnapshotRequestGeneration,
    bool bExpectedConflictReconcile,
    FGamePlatformInventorySnapshot NewSnapshot,
    EGamePlatformInventoryError Error)
{
    if (ExpectedGeneration != AccountGeneration ||
        ExpectedSnapshotRequestGeneration !=
            SnapshotRequestGeneration)
    {
        return;
    }

    bSnapshotRequestInFlight = false;

    if (Error != EGamePlatformInventoryError::None)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            Error);
        return;
    }

    if (!ApplySnapshot(MoveTemp(NewSnapshot)))
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    if (bExpectedConflictReconcile &&
        bConflictSnapshotReconcile &&
        Pending.Type !=
            EGamePlatformInventoryOperationType::None)
    {
        Pending.Reset();
        bConflictSnapshotReconcile = false;
    }

    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

void UGamePlatformInventoryClientSubsystem::
HandleMutationCompleted(
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

    if (Error ==
        EGamePlatformInventoryError::RevisionConflict)
    {
        bConflictSnapshotReconcile = true;
        SetState(
            EGamePlatformInventoryClientState::Reconciling,
            Error);

        if (!RefreshSnapshot())
        {
            SetState(
                EGamePlatformInventoryClientState::Error,
                EGamePlatformInventoryError::BackendUnavailable);
        }
        return;
    }

    if (Error != EGamePlatformInventoryError::None)
    {
        if (IsOutcomeUnknownError(Error))
        {
            SetState(
                EGamePlatformInventoryClientState::Error,
                Error);
            return;
        }

        Pending.Reset();
        bConflictSnapshotReconcile = false;

        SetState(
            Error == EGamePlatformInventoryError::Unauthorized
                ? EGamePlatformInventoryClientState::Error
                : EGamePlatformInventoryClientState::Ready,
            Error);
        return;
    }

    if (!Result.OperationId.IsValid() ||
        Result.OperationId != ExpectedOperationId ||
        !ApplySnapshot(MoveTemp(Result.Snapshot)))
    {
        // 服务端可能已提交；保留原 OperationId，要求查询持久结果而不是生成新操作。
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::InvalidResponse);
        return;
    }

    Pending.Reset();
    bConflictSnapshotReconcile = false;
    SetState(
        EGamePlatformInventoryClientState::Ready,
        EGamePlatformInventoryError::None);
}

bool UGamePlatformInventoryClientSubsystem::ApplySnapshot(
    FGamePlatformInventorySnapshot NewSnapshot)
{
    if (NewSnapshot.InventoryRevision <
            Snapshot.InventoryRevision ||
        NewSnapshot.InventoryRevision <= 0 ||
        NewSnapshot.Containers.Num() >
            MaxInventoryContainers ||
        NewSnapshot.Items.Num() >
            MaxInventoryItems ||
        NewSnapshot.Quickbar.Num() >
            MaxInventoryQuickbarSlots)
    {
        return false;
    }

    TMap<FName, int32> CapacityByContainer;
    CapacityByContainer.Reserve(
        NewSnapshot.Containers.Num());

    for (const FGamePlatformInventoryContainerSnapshot&
             Container : NewSnapshot.Containers)
    {
        if (!Container.IsValid() ||
            CapacityByContainer.Contains(
                Container.ContainerId))
        {
            return false;
        }

        CapacityByContainer.Add(
            Container.ContainerId,
            Container.Capacity);
    }

    TSet<FString> ItemInstanceIds;
    ItemInstanceIds.Reserve(NewSnapshot.Items.Num());

    TSet<FInventorySlotKey> OccupiedSlots;
    OccupiedSlots.Reserve(NewSnapshot.Items.Num());

    for (const FGamePlatformInventoryItemInstance& Item :
         NewSnapshot.Items)
    {
        if (!Item.IsValid() ||
            ItemInstanceIds.Contains(
                Item.ItemInstanceId))
        {
            return false;
        }

        if (CapacityByContainer.Num() > 0)
        {
            const int32* Capacity =
                CapacityByContainer.Find(
                    Item.ContainerId);

            if (!Capacity ||
                Item.SlotIndex >= *Capacity)
            {
                return false;
            }
        }

        const FInventorySlotKey SlotKey{
            Item.ContainerId,
            Item.SlotIndex};

        if (OccupiedSlots.Contains(SlotKey))
        {
            return false;
        }

        ItemInstanceIds.Add(Item.ItemInstanceId);
        OccupiedSlots.Add(SlotKey);
    }

    TSet<int32> QuickbarSlots;
    QuickbarSlots.Reserve(
        NewSnapshot.Quickbar.Num());

    for (const FGamePlatformInventoryQuickbarSlot& Slot :
         NewSnapshot.Quickbar)
    {
        if (Slot.SlotIndex < 0 ||
            Slot.SlotIndex >=
                MaxInventoryQuickbarSlots ||
            Slot.Revision <= 0 ||
            Slot.ItemInstanceId.IsEmpty() ||
            QuickbarSlots.Contains(
                Slot.SlotIndex) ||
            !ItemInstanceIds.Contains(
                Slot.ItemInstanceId))
        {
            return false;
        }

        QuickbarSlots.Add(Slot.SlotIndex);
    }

    Snapshot = MoveTemp(NewSnapshot);
    ++SnapshotGeneration;
    return true;
}

bool UGamePlatformInventoryClientSubsystem::
IsOutcomeUnknownError(
    EGamePlatformInventoryError Error)
{
    switch (Error)
    {
    case EGamePlatformInventoryError::OutcomeUnknown:
    case EGamePlatformInventoryError::BackendUnavailable:
    case EGamePlatformInventoryError::TimedOut:
    case EGamePlatformInventoryError::Cancelled:
    case EGamePlatformInventoryError::OperationInProgress:
    case EGamePlatformInventoryError::InvalidResponse:
        return true;

    default:
        return false;
    }
}

void UGamePlatformInventoryClientSubsystem::SetState(
    EGamePlatformInventoryClientState NewState,
    EGamePlatformInventoryError Error)
{
    const EGamePlatformInventoryClientState OldState =
        State;

    State = NewState;
    LastError = Error;

    UE_LOG(
        LogGamePlatformInventory,
        Verbose,
        TEXT(
            "Inventory state %d -> %d, error=%d, revision=%lld, pending=%s"),
        static_cast<int32>(OldState),
        static_cast<int32>(State),
        static_cast<int32>(LastError),
        static_cast<long long>(
            Snapshot.InventoryRevision),
        *Pending.OperationId.ToString(
            EGuidFormats::DigitsWithHyphensLower));

    OnChanged.Broadcast();
}
