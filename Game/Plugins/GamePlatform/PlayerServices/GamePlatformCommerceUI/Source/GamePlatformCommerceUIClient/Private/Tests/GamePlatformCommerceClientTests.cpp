#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformCommerceClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformCommerceClientSubsystem.h"

namespace
{
class FCommerceMockTransport final
    : public IGamePlatformCommerceClientTransport
{
public:
    FGamePlatformCommerceCatalogCompletion CatalogCompletion;
    FGamePlatformCommerceIntentCompletion IntentCompletion;
    FGamePlatformCommerceOrderCompletion OrderCompletion;

    virtual void CancelAllRequests() override
    {
        CatalogCompletion = {};
        IntentCompletion = {};
        OrderCompletion = {};
    }

    virtual bool BeginGetCatalog(
        FGamePlatformCommerceCatalogCompletion Completion) override
    {
        CatalogCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginCreatePurchaseIntent(
        FName,
        int32,
        const FGuid&,
        FGamePlatformCommerceIntentCompletion Completion) override
    {
        IntentCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginPurchase(
        const FString&,
        FGamePlatformCommerceOrderCompletion Completion) override
    {
        OrderCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginGetOrder(
        const FString&,
        FGamePlatformCommerceOrderCompletion Completion) override
    {
        OrderCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginSubmitReceipt(
        const FString&,
        const FString&,
        FGamePlatformCommerceOrderCompletion Completion) override
    {
        OrderCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginReconcileOrder(
        const FString&,
        FGamePlatformCommerceOrderCompletion Completion) override
    {
        OrderCompletion = MoveTemp(Completion);
        return true;
    }
};

FGamePlatformCommerceCatalogSnapshot CommerceCatalog(int64 Revision)
{
    FGamePlatformCommerceCatalogSnapshot Value;
    Value.CatalogRevision = Revision;
    Value.GeneratedAtUtc = FDateTime(2026,9,24,12,0,0);
    Value.ServerTimeUtc = Value.GeneratedAtUtc;
    return Value;
}

FGamePlatformCommercePurchaseIntentView CommerceIntent()
{
    FGamePlatformCommercePurchaseIntentView Value;
    Value.PurchaseIntentId = TEXT("intent-1");
    Value.RequestId = TEXT("request-1");
    Value.ProductId = TEXT("Product");
    Value.OfferId = TEXT("Offer");
    Value.Quantity = 1;
    Value.CatalogRevision = 1;
    Value.OfferRevision = 1;
    Value.TotalAmountMinor = 199;
    Value.ExpiresAtUtc = FDateTime(2026,9,24,12,10,0);
    return Value;
}

FGamePlatformCommerceOrderStatusView CommerceOrder(
    const TCHAR* OrderState,
    const TCHAR* PaymentState,
    const TCHAR* FulfillmentState)
{
    FGamePlatformCommerceOrderStatusView Value;
    Value.OrderId = TEXT("order-1");
    Value.PurchaseIntentId = TEXT("intent-1");
    Value.ProductId = TEXT("Product");
    Value.OfferId = TEXT("Offer");
    Value.Quantity = 1;
    Value.OrderState = OrderState;
    Value.PaymentState = PaymentState;
    Value.FulfillmentState = FulfillmentState;
    Value.OrderRevision = 1;
    return Value;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCommerceClientFlowTest,
    "GamePlatform.Commerce.Client.BackendAuthority",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformCommerceClientFlowTest::RunTest(const FString&)
{
    UGamePlatformCommerceClientSubsystem* Client =
        NewObject<UGamePlatformCommerceClientSubsystem>();

    TSharedPtr<FCommerceMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FCommerceMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("账号配置启动Catalog"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    Transport->CatalogCompletion(
        CommerceCatalog(1),
        EGamePlatformCommerceError::None);

    const FGuid RequestId = FGuid::NewGuid();

    TestTrue(
        TEXT("首次CreateIntent启动"),
        Client->CreatePurchaseIntent(
            TEXT("Offer"),
            1,
            RequestId));

    TestFalse(
        TEXT("双击CreateIntent被本地Debounce"),
        Client->CreatePurchaseIntent(
            TEXT("Offer"),
            1,
            RequestId));

    Transport->IntentCompletion(
        CommerceIntent(),
        EGamePlatformCommerceError::None);

    TestTrue(
        TEXT("BeginPurchase启动"),
        Client->BeginPurchase(TEXT("intent-1")));

    Transport->OrderCompletion(
        CommerceOrder(
            TEXT("awaiting_payment"),
            TEXT("pending"),
            TEXT("not_started")),
        EGamePlatformCommerceError::None);

    TestEqual(
        TEXT("等待Provider而非成功"),
        Client->GetState(),
        EGamePlatformCommerceClientState::AwaitingProvider);

    TestTrue(
        TEXT("提交Receipt仅进入验证"),
        Client->SubmitReceipt(
            TEXT("order-1"),
            TEXT("opaque-receipt")));

    Transport->OrderCompletion(
        CommerceOrder(
            TEXT("fulfillment_pending"),
            TEXT("confirmed"),
            TEXT("pending")),
        EGamePlatformCommerceError::None);

    TestEqual(
        TEXT("支付已确认但履约未完成不能显示Succeeded"),
        Client->GetState(),
        EGamePlatformCommerceClientState::AwaitingFulfillment);

    TestTrue(
        TEXT("Reconcile启动"),
        Client->ReconcileOrder(TEXT("order-1")));

    Transport->OrderCompletion(
        CommerceOrder(
            TEXT("fulfilled"),
            TEXT("confirmed"),
            TEXT("fulfilled")),
        EGamePlatformCommerceError::None);

    TestEqual(
        TEXT("只有后端Fulfilled才成功"),
        Client->GetState(),
        EGamePlatformCommerceClientState::Succeeded);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCommerceAccountIsolationTest,
    "GamePlatform.Commerce.Client.AccountIsolation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformCommerceAccountIsolationTest::RunTest(const FString&)
{
    UGamePlatformCommerceClientSubsystem* Client =
        NewObject<UGamePlatformCommerceClientSubsystem>();

    TSharedPtr<FCommerceMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FCommerceMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("账号A启动Catalog"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    FGamePlatformCommerceCatalogCompletion LateCatalog =
        MoveTemp(Transport->CatalogCompletion);

    Client->ResetAccount();

    LateCatalog(
        CommerceCatalog(9),
        EGamePlatformCommerceError::None);

    TestEqual(
        TEXT("旧账号回调不能污染缓存"),
        Client->GetCatalogRevision(),
        int64(0));

    return true;
}

#endif
