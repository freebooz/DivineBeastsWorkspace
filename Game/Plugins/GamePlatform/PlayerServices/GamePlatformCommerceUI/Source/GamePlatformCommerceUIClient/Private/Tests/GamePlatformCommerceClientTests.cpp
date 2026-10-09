// 平台玩家服务Automation回归：测试Transport仅控制完成/取消，不访问生产路由；验证状态、账号隔离、广播重置与后端权威显示。
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


// 验证公开状态监听器在Loading内重置账号：受理失败、旧传输没有请求、清空后的账号状态不被旧栈覆盖。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommerceResetDuringStateTest, "GamePlatform.Commerce.Client.ResetDuringState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommerceResetDuringStateTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformCommerceClientSubsystem>();
    auto Transport = MakeShared<FCommerceMockTransport, ESPMode::ThreadSafe>();
    Client->OnCommerceStateChanged.AddLambda([Client](auto&&...)
    {
        if (Client->GetState() == EGamePlatformCommerceClientState::LoadingCatalog) { Client->ResetAccount(); }
    });
    TestFalse(TEXT("Loading监听器重置后不得接纳旧账号请求"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Reset"), Transport));
    TestFalse(TEXT("传输未启动失效请求"), static_cast<bool>(Transport->CatalogCompletion));
    TestEqual(TEXT("回调返回后保持清空状态"), Client->GetState(), EGamePlatformCommerceClientState::Idle);
    return true;
}


// 生命周期回归：测试Transport不访问网络；Reset同步通知调用Deinitialize后，关闭作用域必须拒绝恢复账号及公开刷新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommerceCloseDuringConfigureTest, "GamePlatform.Commerce.Client.CloseDuringConfigure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommerceCloseDuringConfigureTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformCommerceClientSubsystem>();
    auto Transport = MakeShared<FCommerceMockTransport, ESPMode::ThreadSafe>();
    TestTrue(TEXT("前置账号请求成功受理"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Previous"), Transport));
    Client->OnCommerceStateChanged.AddLambda([Client](auto&&...) { Client->Deinitialize(); });
    TestFalse(TEXT("Reset通知内关闭后Configure不得复活服务"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Closed"), Transport));
    TestFalse(TEXT("关闭后不得启动请求"), static_cast<bool>(Transport->CatalogCompletion));
    TestFalse(TEXT("公开刷新拒绝已关闭作用域"), Client->RefreshCatalog());
    return true;
}

#endif
