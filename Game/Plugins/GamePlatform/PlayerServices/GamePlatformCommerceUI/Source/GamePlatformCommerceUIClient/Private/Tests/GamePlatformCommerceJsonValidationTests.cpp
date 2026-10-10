// 真实JSON解析回归：仅测试夹具，不访问网络；调用生产HTTP适配的私有解析器，验证畸形输入不能发布成功空快照。
#include "Transport/GamePlatformCommerceGatewayHttpTransport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformCommerceJsonValidationTest, "GamePlatform.Commerce.Client.JsonValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformCommerceJsonValidationTest::RunTest(const FString&)
{
    auto Parse = [](const FString& Body) { TSharedPtr<FJsonObject> Json; auto Reader = TJsonReaderFactory<>::Create(Body); FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings); return Json; };

    for (const auto* Token : {TEXT("100.5"), TEXT("9223372036854775808"), TEXT("1e100")})
    {
        FGamePlatformCommercePriceView Price;
        TestFalse(TEXT("金额小数/溢出必须拒绝"), FGamePlatformCommerceGatewayHttpTransport::JsonToPrice(Parse(FString::Printf(TEXT("{\"price_id\":\"p\",\"price_type\":\"real_money\",\"unit_amount_minor\":%s}"), Token)), Price, nullptr));
    }
    FGamePlatformCommercePriceView Price;
    TestTrue(TEXT("int64上界原始数字保真"), FGamePlatformCommerceGatewayHttpTransport::JsonToPrice(Parse(TEXT("{\"price_id\":\"p\",\"price_type\":\"real_money\",\"unit_amount_minor\":9223372036854775807}")), Price, nullptr));
    TestEqual(TEXT("金额不可静默丢精度"), Price.UnitAmountMinor, int64(MAX_int64));
    int64 Total = 0;
    TestFalse(TEXT("明确非法总额不得回退单价"), FGamePlatformCommerceGatewayHttpTransport::JsonToPrice(Parse(TEXT("{\"price_id\":\"p\",\"price_type\":\"free\",\"unit_amount_minor\":0,\"total_amount_minor\":100.5}")), Price, &Total));
    for (const auto* Field : {TEXT("prices"), TEXT("products"), TEXT("offers")})
    {
        FGamePlatformCommerceCatalogSnapshot Catalog;
        TestFalse(TEXT("明确错类型目录集合必须拒绝"), FGamePlatformCommerceGatewayHttpTransport::JsonToCatalog(Parse(FString::Printf(TEXT("{\"server_time_utc\":\"2026-09-30T00:00:00Z\",\"catalog\":{\"catalog_revision\":1,\"generated_at\":\"2026-09-30T00:00:00Z\",\"%s\":{}}}"), Field)), Catalog));
    }
    const FString IntentBody = TEXT("{\"purchase_intent_id\":\"i\",\"request_id\":\"r\",\"product_id\":\"p\",\"offer_id\":\"o\",\"quantity\":1.5,\"catalog_revision\":1,\"offer_revision\":1,\"expires_at\":\"2026-09-30T00:00:00Z\",\"price_snapshot\":{\"price_id\":\"p\",\"price_type\":\"free\",\"unit_amount_minor\":0}}");
    FGamePlatformCommercePurchaseIntentView Intent;
    TestFalse(TEXT("购买数量不可截断"), FGamePlatformCommerceGatewayHttpTransport::JsonToIntent(Parse(IntentBody), Intent));
    TestTrue(TEXT("合法数量仍可解析"), FGamePlatformCommerceGatewayHttpTransport::JsonToIntent(Parse(IntentBody.Replace(TEXT("1.5"), TEXT("1"))), Intent));

    const FString OrderBody = TEXT("{\"order_id\":\"order\",\"purchase_intent_id\":\"intent\",\"product_id\":\"p\",\"offer_id\":\"o\",\"quantity\":1.5,\"order_state\":\"fulfilled\",\"payment_state\":\"confirmed\",\"fulfillment_state\":\"fulfilled\",\"order_revision\":1,\"price_snapshot\":{\"price_id\":\"p\",\"price_type\":\"free\",\"unit_amount_minor\":0}}");
    FGamePlatformCommerceOrderStatusView Order;
    TestFalse(TEXT("订单数量小数拒绝"), FGamePlatformCommerceGatewayHttpTransport::JsonToOrder(Parse(OrderBody), Order));
    TestFalse(TEXT("订单数量int32溢出拒绝"), FGamePlatformCommerceGatewayHttpTransport::JsonToOrder(Parse(OrderBody.Replace(TEXT("1.5"), TEXT("2147483648"))), Order));
    TestTrue(TEXT("合法订单仍可解析"), FGamePlatformCommerceGatewayHttpTransport::JsonToOrder(Parse(OrderBody.Replace(TEXT("1.5"), TEXT("1"))), Order));
    return true;
}
#endif
