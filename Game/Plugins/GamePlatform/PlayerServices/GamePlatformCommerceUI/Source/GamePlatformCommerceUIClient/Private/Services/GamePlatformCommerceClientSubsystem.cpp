// 平台本地玩家商店投影实现：游戏线程事件与异步领域Transport，Online拥有认证；账号重置清空本地缓存并失效本领域回调。
#include "Services/GamePlatformCommerceClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Transport/GamePlatformCommerceGatewayHttpTransport.h"

#include "Interfaces/GamePlatformCommerceClientTransport.h"
#include "ViewModels/GamePlatformCommerceViewModel.h"

void UGamePlatformCommerceClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    // Initialize建立新实例代次；退出后的迟到回调不能跨重新初始化消费。
    bDeinitializing = false; ++InstanceGeneration; ++AccountGeneration;
    Super::Initialize(Collection);

    ViewModel =
        NewObject<UGamePlatformCommerceViewModel>(this);
    BindOnlineAuthentication();
}

void UGamePlatformCommerceClientSubsystem::Deinitialize()
{
    // 先关闭作用域，再执行任何取消或广播；外部回调不得恢复账号。
    bDeinitializing = true; ++InstanceGeneration; ++AccountGeneration;
    UnbindOnlineAuthentication();
    OnCommerceStateChanged.Clear(); OnOrderChanged.Clear();
    const auto CancelledTransport = Transport;
    if (CancelledTransport.IsValid())
    {
        CancelledTransport->CancelAllRequests();
    }

    ++AccountGeneration;
    ++CatalogRequestGeneration; ++IntentRequestGeneration; ++OrderRequestGeneration;
    CurrentAccountKey.Reset();
    Transport.Reset();
    Catalog = {};
    CurrentIntent = {};
    CurrentOrder = {};
    ViewModel = nullptr;
    State = EGamePlatformCommerceClientState::Idle;
    LastError = EGamePlatformCommerceError::None;
    bCatalogRequestInFlight = false;
    bIntentRequestInFlight = false;
    bOrderRequestInFlight = false;

    Super::Deinitialize();
}

bool UGamePlatformCommerceClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformCommerceClientTransport, ESPMode::ThreadSafe>
        InTransport)
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
    return RefreshCatalog();
}

void UGamePlatformCommerceClientSubsystem::ResetAccount()
{
    check(IsInGameThread());
    if (bResettingAccount) { return; }
    TGuardValue<bool> ResetGuard(bResettingAccount, true);
    ++AccountGeneration;
    ++CatalogRequestGeneration; ++IntentRequestGeneration; ++OrderRequestGeneration;

    const auto CancelledTransport = Transport;
    if (CancelledTransport.IsValid())
    {
        CancelledTransport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Transport.Reset();

    CurrentIntent = {};
    CurrentOrder = {};
    if (ViewModel)
    {
        ViewModel->ClearPlayerState();
    }

    bCatalogRequestInFlight = false;
    bIntentRequestInFlight = false;
    bOrderRequestInFlight = false;

    LastError = EGamePlatformCommerceError::None;
    SetState(
        Catalog.CatalogRevision > 0
            ? EGamePlatformCommerceClientState::Browsing
            : EGamePlatformCommerceClientState::Idle);
}

bool UGamePlatformCommerceClientSubsystem::RefreshCatalog()
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        bCatalogRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++CatalogRequestGeneration;
    bCatalogRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::LoadingCatalog);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetCatalog(
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommerceCatalogSnapshot Snapshot,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleCatalog(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Snapshot),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == CatalogRequestGeneration && bCatalogRequestInFlight)
    {
        bCatalogRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }

    return bStarted;
}

bool UGamePlatformCommerceClientSubsystem::CreatePurchaseIntent(
    FName OfferId,
    int32 Quantity,
    const FGuid& RequestId)
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OfferId.IsNone() ||
        Quantity <= 0 ||
        !RequestId.IsValid() ||
        bIntentRequestInFlight ||
        bOrderRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++IntentRequestGeneration;
    bIntentRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::CreatingIntent);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginCreatePurchaseIntent(
            OfferId,
            Quantity,
            RequestId,
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommercePurchaseIntentView Intent,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleIntent(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Intent),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == IntentRequestGeneration && bIntentRequestInFlight)
    {
        bIntentRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }

    return bStarted;
}

bool UGamePlatformCommerceClientSubsystem::BeginPurchase(
    const FString& PurchaseIntentId)
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        PurchaseIntentId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++OrderRequestGeneration;
    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Confirming);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginPurchase(
            PurchaseIntentId,
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == OrderRequestGeneration && bOrderRequestInFlight)
    {
        bOrderRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }
    return bStarted;
}

bool UGamePlatformCommerceClientSubsystem::SubmitReceipt(
    const FString& OrderId,
    const FString& Receipt)
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        Receipt.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++OrderRequestGeneration;
    bOrderRequestInFlight = true;

    // Receipt（支付凭据）只是待验证输入。
    // 客户端绝不能因为Provider SDK返回Success就进入Succeeded。
    SetState(EGamePlatformCommerceClientState::AwaitingVerification);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginSubmitReceipt(
            OrderId,
            Receipt,
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == OrderRequestGeneration && bOrderRequestInFlight)
    {
        bOrderRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }
    return bStarted;
}

bool UGamePlatformCommerceClientSubsystem::RefreshOrder(
    const FString& OrderId)
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++OrderRequestGeneration;
    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Reconciling);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetOrder(
            OrderId,
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == OrderRequestGeneration && bOrderRequestInFlight)
    {
        bOrderRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }
    return bStarted;
}

bool UGamePlatformCommerceClientSubsystem::ReconcileOrder(
    const FString& OrderId)
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const auto RequestTransport = Transport;
    const uint64 ExpectedRequestGeneration = ++OrderRequestGeneration;
    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Reconciling);

    // 状态/ViewModel广播可以重置或切换账号，禁止沿旧调用栈发送交易命令。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginReconcileOrder(
            OrderId,
            [WeakThis, ExpectedGeneration, ExpectedRequestGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        ExpectedRequestGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && RequestTransport == Transport && ExpectedRequestGeneration == OrderRequestGeneration && bOrderRequestInFlight)
    {
        bOrderRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }
    return bStarted;
}

void UGamePlatformCommerceClientSubsystem::HandleCatalog(
    uint64 ExpectedGeneration,
    uint64 ExpectedRequestGeneration,
    FGamePlatformCommerceCatalogSnapshot Snapshot,
    EGamePlatformCommerceError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != CatalogRequestGeneration || !bCatalogRequestInFlight)
    {
        return;
    }

    bCatalogRequestInFlight = false;

    if (Error != EGamePlatformCommerceError::None ||
        Snapshot.CatalogRevision < 1)
    {
        LastError =
            Error != EGamePlatformCommerceError::None
                ? Error
                : EGamePlatformCommerceError::InvalidResponse;
        SetState(EGamePlatformCommerceClientState::Failed);
        return;
    }

    if (Snapshot.CatalogRevision >= Catalog.CatalogRevision)
    {
        Catalog = MoveTemp(Snapshot);
        if (ViewModel)
        {
            ViewModel->ApplyCatalog(Catalog);
            if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != CatalogRequestGeneration) { return; }
        }
    }

    LastError = EGamePlatformCommerceError::None;
    SetState(EGamePlatformCommerceClientState::Browsing);
}

void UGamePlatformCommerceClientSubsystem::HandleIntent(
    uint64 ExpectedGeneration,
    uint64 ExpectedRequestGeneration,
    FGamePlatformCommercePurchaseIntentView Intent,
    EGamePlatformCommerceError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != IntentRequestGeneration || !bIntentRequestInFlight)
    {
        return;
    }

    bIntentRequestInFlight = false;

    if (Error != EGamePlatformCommerceError::None ||
        Intent.PurchaseIntentId.IsEmpty())
    {
        LastError =
            Error != EGamePlatformCommerceError::None
                ? Error
                : EGamePlatformCommerceError::InvalidResponse;
        SetState(EGamePlatformCommerceClientState::Failed);
        return;
    }

    CurrentIntent = MoveTemp(Intent);
    if (ViewModel)
    {
        ViewModel->ApplyIntent(CurrentIntent);
        if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != IntentRequestGeneration) { return; }
    }

    LastError = EGamePlatformCommerceError::None;
    SetState(EGamePlatformCommerceClientState::Confirming);
}

void UGamePlatformCommerceClientSubsystem::HandleOrder(
    uint64 ExpectedGeneration,
    uint64 ExpectedRequestGeneration,
    FGamePlatformCommerceOrderStatusView Order,
    EGamePlatformCommerceError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OrderRequestGeneration || !bOrderRequestInFlight)
    {
        return;
    }

    bOrderRequestInFlight = false;

    if (Error != EGamePlatformCommerceError::None ||
        Order.OrderId.IsEmpty())
    {
        LastError =
            Error != EGamePlatformCommerceError::None
                ? Error
                : EGamePlatformCommerceError::InvalidResponse;

        if (Error == EGamePlatformCommerceError::OutcomeUnknown ||
            Error == EGamePlatformCommerceError::TimedOut ||
            Error == EGamePlatformCommerceError::BackendUnavailable)
        {
            const bool bWasAlreadyReconciling =
                State == EGamePlatformCommerceClientState::Reconciling;

            SetState(EGamePlatformCommerceClientState::Reconciling);
            if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OrderRequestGeneration) { return; }

            if (!bWasAlreadyReconciling &&
                !CurrentOrder.OrderId.IsEmpty())
            {
                ReconcileOrder(CurrentOrder.OrderId);
            }
        }
        else
        {
            SetState(EGamePlatformCommerceClientState::Failed);
        }
        return;
    }

    CurrentOrder = Order;
    if (ViewModel)
    {
        ViewModel->ApplyOrder(CurrentOrder);
        if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OrderRequestGeneration) { return; }
    }

    const auto AcceptedOrder = CurrentOrder;
    OnOrderChanged.Broadcast(AcceptedOrder);
    if (bDeinitializing || ExpectedGeneration != AccountGeneration || ExpectedRequestGeneration != OrderRequestGeneration) { return; }
    LastError = EGamePlatformCommerceError::None;
    ApplyOrderState(CurrentOrder);
}

void UGamePlatformCommerceClientSubsystem::SetState(
    EGamePlatformCommerceClientState NewState)
{
    if (bDeinitializing) { return; }
    if (State == NewState)
    {
        return;
    }
    State = NewState;
    OnCommerceStateChanged.Broadcast();
}

void UGamePlatformCommerceClientSubsystem::ApplyOrderState(
    const FGamePlatformCommerceOrderStatusView& Order)
{
    // 唯一成功条件：Backend Order=fulfilled。
    if (Order.OrderState == TEXT("fulfilled") &&
        Order.PaymentState == TEXT("confirmed") &&
        Order.FulfillmentState == TEXT("fulfilled"))
    {
        SetState(EGamePlatformCommerceClientState::Succeeded);
        return;
    }

    if (Order.PaymentState == TEXT("confirmed") &&
        Order.FulfillmentState != TEXT("fulfilled"))
    {
        SetState(EGamePlatformCommerceClientState::AwaitingFulfillment);
        return;
    }

    if (Order.OrderState == TEXT("awaiting_payment"))
    {
        SetState(EGamePlatformCommerceClientState::AwaitingProvider);
        return;
    }

    if (Order.OrderState == TEXT("payment_failed") ||
        Order.OrderState == TEXT("fulfillment_failed") ||
        Order.OrderState == TEXT("cancelled") ||
        Order.OrderState == TEXT("expired") ||
        Order.OrderState == TEXT("refunded"))
    {
        SetState(EGamePlatformCommerceClientState::Failed);
        return;
    }

    SetState(EGamePlatformCommerceClientState::Reconciling);
}

// Online负责认证/刷新/请求签名；领域层只消费脱敏认证快照，缺失路由仍走真实失败终态。
void UGamePlatformCommerceClientSubsystem::BindOnlineAuthentication()
{
    if (bDeinitializing) { return; }
    auto* LocalPlayer = GetLocalPlayer();
    auto* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    if (!IsValid(Online)) { return; }
    OnlineSubsystem = Online;
    if (!AuthStateChangedHandle.IsValid())
    { AuthStateChangedHandle = Online->OnAuthStateChanged().AddUObject(this, &UGamePlatformCommerceClientSubsystem::HandleAuthStateChanged); }
    HandleAuthStateChanged(Online->GetSnapshot());
}

void UGamePlatformCommerceClientSubsystem::UnbindOnlineAuthentication()
{
    if (auto* Online = OnlineSubsystem.Get())
    { if (AuthStateChangedHandle.IsValid()) { Online->OnAuthStateChanged().Remove(AuthStateChangedHandle); } }
    AuthStateChangedHandle.Reset(); OnlineSubsystem.Reset();
}

void UGamePlatformCommerceClientSubsystem::HandleAuthStateChanged(const FGamePlatformAuthSnapshot& AuthSnapshot)
{
    if (bDeinitializing) { return; }
    check(IsInGameThread());
    if (AuthSnapshot.State == EGamePlatformAuthState::Refreshing) { return; }
    if (AuthSnapshot.State == EGamePlatformAuthState::Authenticated)
    {
        auto* Online = OnlineSubsystem.Get();
        if (!Online || AuthSnapshot.AccountId.IsEmpty()) { ResetAccount(); return; }
        if (CurrentAccountKey == AuthSnapshot.AccountId && Transport.IsValid()) { return; }
        ConfigureAuthenticatedAccount(AuthSnapshot.AccountId,
            MakeShared<FGamePlatformCommerceGatewayHttpTransport, ESPMode::ThreadSafe>(Online));
        return;
    }
    if (!CurrentAccountKey.IsEmpty() || Transport.IsValid()) { ResetAccount(); }
}
