// 平台本地玩家背包投影实现：游戏线程事件与异步领域Transport，Online拥有认证；账号重置清空本地缓存并失效本领域回调。
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
    // Initialize建立新实例代次；退出后的迟到回调不能跨重新初始化消费。
    bDeinitializing = false; ++InstanceGeneration; ++AccountGeneration;
    Super::Initialize(Collection);
    BindOnlineAuthentication();
}

void UGamePlatformInventoryClientSubsystem::Deinitialize()
{
    // 先关闭作用域，再执行任何取消或广播；外部回调不得恢复账号。
    bDeinitializing = true; ++InstanceGeneration; ++AccountGeneration;
    UnbindOnlineAuthentication();
    OnChanged.Clear();
    ResetAccount();
    State = EGamePlatformInventoryClientState::Uninitialized;
    Super::Deinitialize();
}

void UGamePlatformInventoryClientSubsystem::BindOnlineAuthentication()
{
    if (bDeinitializing) { return; }
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
    if (bDeinitializing) { return; }
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
    check(IsInGameThread());
    if (bDeinitializing || bResettingAccount || AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    const uint64 ExpectedInstanceGeneration = InstanceGeneration;
    ResetAccount();
    if (bDeinitializing || ExpectedInstanceGeneration != InstanceGeneration) { return false; }
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    return RefreshSnapshot();
}

void UGamePlatformInventoryClientSubsystem::ResetAccount()
{
    ++OperationRequestGeneration; bOperationRequestInFlight = false;
    check(IsInGameThread());
    if (bResettingAccount) { return; }
    TGuardValue<bool> ResetGuard(bResettingAccount, true);
    ++AccountGeneration;
    ++SnapshotRequestGeneration;

    const auto CancelledTransport = Transport;
    if (CancelledTransport.IsValid())
    {
        CancelledTransport->CancelAllRequests();
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
    if (bDeinitializing) { return false; }
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
    const auto RequestTransport = Transport;

    bSnapshotRequestInFlight = true;

    SetState(
        bConflictReconcile
            ? EGamePlatformInventoryClientState::Reconciling
            : EGamePlatformInventoryClientState::Loading,
        bConflictReconcile
            ? EGamePlatformInventoryError::RevisionConflict
            : EGamePlatformInventoryError::None);

    // Loading监听器可以同步ResetAccount/切换账号；旧传输快照只保活，不允许启动已失效请求。
    if (ExpectedGeneration != AccountGeneration || ExpectedSnapshotRequestGeneration != SnapshotRequestGeneration || RequestTransport != Transport)
    {
        return false;
    }
    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetSnapshot(
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
            SnapshotRequestGeneration && bSnapshotRequestInFlight)
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
    if (bDeinitializing) { return {}; }
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
    if (bDeinitializing) { return {}; }
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
    if (bDeinitializing) { return {}; }
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
    if (bDeinitializing) { return {}; }
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
    if (bDeinitializing) { return {}; }
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
    if (bDeinitializing) { return false; }
    if (!Transport.IsValid() ||
        Pending.Type ==
            EGamePlatformInventoryOperationType::None ||
        !Pending.OperationId.IsValid())
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const FGuid ExpectedOperationId = Pending.OperationId;
    const auto RequestTransport = Transport;
    const auto Request = Pending;
    const uint64 ExpectedRequestGeneration = ++OperationRequestGeneration;
    bOperationRequestInFlight = true;
    SetState(EGamePlatformInventoryClientState::Mutating, EGamePlatformInventoryError::None);
    if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedOperationId != Pending.OperationId || RequestTransport != Transport) { return false; }

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    FGamePlatformInventoryMutationCompletion Completion =
        [WeakThis,
         ExpectedGeneration,
         ExpectedOperationId, ExpectedRequestGeneration](
            FGamePlatformInventoryMutationResult Result,
            EGamePlatformInventoryError Error)
        {
            if (UGamePlatformInventoryClientSubsystem* Self =
                    WeakThis.Get())
            {
                if (Self->bDeinitializing || Self->AccountGeneration != ExpectedGeneration || Self->OperationRequestGeneration != ExpectedRequestGeneration || !Self->bOperationRequestInFlight || Self->Pending.OperationId != ExpectedOperationId) { return; }
                Self->bOperationRequestInFlight = false;
                Self->HandleMutationCompleted(
                    ExpectedGeneration,
                    ExpectedRequestGeneration,
                    ExpectedOperationId,
                    MoveTemp(Result),
                    Error);
            }
        };

    bool bStarted = false;

    switch (Request.Type)
    {
    case EGamePlatformInventoryOperationType::Move:
        bStarted =
            RequestTransport->BeginMove(
                Request.Move,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::Split:
        bStarted =
            RequestTransport->BeginSplit(
                Request.Split,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::Merge:
        bStarted =
            RequestTransport->BeginMerge(
                Request.Merge,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::SetQuickbar:
        bStarted =
            RequestTransport->BeginSetQuickbar(
                Request.Quickbar,
                MoveTemp(Completion));
        break;

    case EGamePlatformInventoryOperationType::ClearQuickbar:
        bStarted =
            RequestTransport->BeginClearQuickbar(
                Request.Quickbar,
                MoveTemp(Completion));
        break;

    default:
        break;
    }

    // 同步终态已经消费门闩时返回栈只能返回受理结果，不能再次改变领域状态。
    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && ExpectedOperationId == Pending.OperationId && ExpectedRequestGeneration == OperationRequestGeneration && bOperationRequestInFlight)
    { bOperationRequestInFlight = false; SetState(EGamePlatformInventoryClientState::Error, EGamePlatformInventoryError::BackendUnavailable); }
    return bStarted;
}

bool UGamePlatformInventoryClientSubsystem::
RetryPendingOperation()
{
    if (bDeinitializing) { return false; }
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
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++OperationRequestGeneration;
    bOperationRequestInFlight = true;
    SetState(EGamePlatformInventoryClientState::Reconciling, EGamePlatformInventoryError::None);
    if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedOperationId != Pending.OperationId || RequestTransport != Transport) { return false; }

    TWeakObjectPtr<UGamePlatformInventoryClientSubsystem>
        WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetOperation(
            ExpectedOperationId,
            [WeakThis,
             ExpectedGeneration,
             ExpectedOperationId, ExpectedRequestGeneration](
                FGamePlatformInventoryMutationResult Result,
                EGamePlatformInventoryError Error)
            {
                if (UGamePlatformInventoryClientSubsystem* Self =
                        WeakThis.Get())
                {
                    if (Self->bDeinitializing || Self->AccountGeneration != ExpectedGeneration || Self->OperationRequestGeneration != ExpectedRequestGeneration || !Self->bOperationRequestInFlight || Self->Pending.OperationId != ExpectedOperationId) { return; }
                    Self->bOperationRequestInFlight = false;
                    Self->HandleOperationQueryCompleted(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        ExpectedOperationId,
                        MoveTemp(Result),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && ExpectedOperationId == Pending.OperationId && ExpectedRequestGeneration == OperationRequestGeneration && bOperationRequestInFlight)
    { bOperationRequestInFlight = false; SetState(EGamePlatformInventoryClientState::Error, EGamePlatformInventoryError::BackendUnavailable); }
    return bStarted;
}

void UGamePlatformInventoryClientSubsystem::
HandleOperationQueryCompleted(
    uint64 ExpectedGeneration,
    uint64 ExpectedRequestGeneration,
    FGuid ExpectedOperationId,
    FGamePlatformInventoryMutationResult Result,
    EGamePlatformInventoryError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OperationRequestGeneration ||
        Pending.OperationId != ExpectedOperationId)
    {
        return;
    }

    const uint64 SnapshotGenerationBeforeNotification = SnapshotRequestGeneration;
    if (Error ==
        EGamePlatformInventoryError::OperationNotFound)
    {
        SetState(
            EGamePlatformInventoryClientState::Error,
            EGamePlatformInventoryError::OutcomeUnknown);

        // 通知监听器可先启动查询B；即使B同步终态已清在飞标记，原请求代次变化也表示接管。
        // 同时复核Snapshot资格，避免旧查询栈覆盖监听器发起的对账。
        if (!bDeinitializing && ExpectedGeneration == AccountGeneration && ExpectedOperationId == Pending.OperationId &&
            ExpectedRequestGeneration == OperationRequestGeneration && !bOperationRequestInFlight &&
            SnapshotGenerationBeforeNotification == SnapshotRequestGeneration && !bSnapshotRequestInFlight)
        { SendPendingOperation(); }
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
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration ||
        ExpectedSnapshotRequestGeneration !=
            SnapshotRequestGeneration || !bSnapshotRequestInFlight)
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
    uint64 ExpectedRequestGeneration,
    FGuid ExpectedOperationId,
    FGamePlatformInventoryMutationResult Result,
    EGamePlatformInventoryError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OperationRequestGeneration ||
        Pending.OperationId != ExpectedOperationId)
    {
        return;
    }

    if (Error ==
        EGamePlatformInventoryError::RevisionConflict)
    {
        const uint64 SnapshotGenerationBeforeNotification = SnapshotRequestGeneration;
        bConflictSnapshotReconcile = true;
        SetState(
            EGamePlatformInventoryClientState::Reconciling,
            Error);

        // 监听器可能已经受理/同步完成Snapshot对账，或改为新的操作查询；旧栈不能抢回资格。
        if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedOperationId != Pending.OperationId ||
            ExpectedRequestGeneration != OperationRequestGeneration || bOperationRequestInFlight ||
            SnapshotGenerationBeforeNotification != SnapshotRequestGeneration || bSnapshotRequestInFlight || !bConflictSnapshotReconcile) { return; }
        // RefreshSnapshot自身负责启动失败终态；false也可能表示被监听器接管，不能统一伪报后端不可用。
        RefreshSnapshot();
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
    if (bDeinitializing) { return; }
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
