#include "Services/GamePlatformCommerceClientSubsystem.h"

#include "Interfaces/GamePlatformCommerceClientTransport.h"
#include "ViewModels/GamePlatformCommerceViewModel.h"

void UGamePlatformCommerceClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ViewModel =
        NewObject<UGamePlatformCommerceViewModel>(this);
}

void UGamePlatformCommerceClientSubsystem::Deinitialize()
{
    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    ++AccountGeneration;
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
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    return RefreshCatalog();
}

void UGamePlatformCommerceClientSubsystem::ResetAccount()
{
    ++AccountGeneration;

    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
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
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        bCatalogRequestInFlight)
    {
        return false;
    }

    bCatalogRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::LoadingCatalog);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetCatalog(
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommerceCatalogSnapshot Snapshot,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleCatalog(
                        ExpectedGeneration,
                        MoveTemp(Snapshot),
                        Error);
                }
            });

    if (!bStarted)
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

    bIntentRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::CreatingIntent);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginCreatePurchaseIntent(
            OfferId,
            Quantity,
            RequestId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommercePurchaseIntentView Intent,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleIntent(
                        ExpectedGeneration,
                        MoveTemp(Intent),
                        Error);
                }
            });

    if (!bStarted)
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
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        PurchaseIntentId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Confirming);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginPurchase(
            PurchaseIntentId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted)
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
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        Receipt.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    bOrderRequestInFlight = true;

    // Receipt（支付凭据）只是待验证输入。
    // 客户端绝不能因为Provider SDK返回Success就进入Succeeded。
    SetState(EGamePlatformCommerceClientState::AwaitingVerification);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginSubmitReceipt(
            OrderId,
            Receipt,
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted)
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
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Reconciling);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetOrder(
            OrderId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted)
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
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        OrderId.IsEmpty() ||
        bOrderRequestInFlight)
    {
        return false;
    }

    bOrderRequestInFlight = true;
    SetState(EGamePlatformCommerceClientState::Reconciling);

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformCommerceClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginReconcileOrder(
            OrderId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformCommerceOrderStatusView Order,
                EGamePlatformCommerceError Error)
            {
                if (UGamePlatformCommerceClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleOrder(
                        ExpectedGeneration,
                        MoveTemp(Order),
                        Error);
                }
            });

    if (!bStarted)
    {
        bOrderRequestInFlight = false;
        LastError = EGamePlatformCommerceError::BackendUnavailable;
        SetState(EGamePlatformCommerceClientState::Failed);
    }
    return bStarted;
}

void UGamePlatformCommerceClientSubsystem::HandleCatalog(
    uint64 ExpectedGeneration,
    FGamePlatformCommerceCatalogSnapshot Snapshot,
    EGamePlatformCommerceError Error)
{
    if (ExpectedGeneration != AccountGeneration)
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
        }
    }

    LastError = EGamePlatformCommerceError::None;
    SetState(EGamePlatformCommerceClientState::Browsing);
}

void UGamePlatformCommerceClientSubsystem::HandleIntent(
    uint64 ExpectedGeneration,
    FGamePlatformCommercePurchaseIntentView Intent,
    EGamePlatformCommerceError Error)
{
    if (ExpectedGeneration != AccountGeneration)
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
    }

    LastError = EGamePlatformCommerceError::None;
    SetState(EGamePlatformCommerceClientState::Confirming);
}

void UGamePlatformCommerceClientSubsystem::HandleOrder(
    uint64 ExpectedGeneration,
    FGamePlatformCommerceOrderStatusView Order,
    EGamePlatformCommerceError Error)
{
    if (ExpectedGeneration != AccountGeneration)
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
    }

    OnOrderChanged.Broadcast(CurrentOrder);
    LastError = EGamePlatformCommerceError::None;
    ApplyOrderState(CurrentOrder);
}

void UGamePlatformCommerceClientSubsystem::SetState(
    EGamePlatformCommerceClientState NewState)
{
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
