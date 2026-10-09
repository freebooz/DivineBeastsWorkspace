// 真实JSON解析回归：仅测试夹具，不访问网络；调用生产HTTP适配的私有解析器，验证畸形输入不能发布成功空快照。
#include "Transport/GamePlatformLiveOpsGatewayHttpTransport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformLiveOpsJsonValidationTest, "GamePlatform.LiveOps.Client.JsonValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformLiveOpsJsonValidationTest::RunTest(const FString&)
{
    auto Parse = [](const FString& Body) { TSharedPtr<FJsonObject> Json; auto Reader = TJsonReaderFactory<>::Create(Body); FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings); return Json; };

    const FString Prefix = TEXT("{\"server_time_utc\":\"2026-09-30T00:00:00Z\",\"catalog\":{");
    FGamePlatformLiveOpsCatalogSnapshot Catalog;
    TestTrue(TEXT("既有合同允许省略三目录集合"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(Prefix + TEXT("\"catalog_version\":1,\"catalog_revision\":1}}")), Catalog));
    for (const auto* Token : {TEXT("1.5"), TEXT("2147483648"), TEXT("1e100")})
    { FGamePlatformLiveOpsCatalogSnapshot Candidate; TestFalse(TEXT("目录版本必须是int32正整数"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(Prefix + FString::Printf(TEXT("\"catalog_version\":%s,\"catalog_revision\":1}}"), Token)), Candidate)); }
    for (const auto* Field : {TEXT("seasons"), TEXT("events"), TEXT("sign_in_campaigns")})
    { FGamePlatformLiveOpsCatalogSnapshot Candidate; TestFalse(TEXT("明确集合错类型必须拒绝"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(Prefix + FString::Printf(TEXT("\"catalog_version\":1,\"catalog_revision\":1,\"%s\":{}}}"), Field)), Candidate)); }
    const FString StatePrefix = TEXT("{\"player_state_revision\":1,\"generated_at\":\"2026-09-30T00:00:00Z\",\"server_time_utc\":\"2026-09-30T00:00:00Z\",\"campaign_states\":");
    FGamePlatformLiveOpsPlayerState State;
    TestFalse(TEXT("campaign_states字符串不得成为成功空快照"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToPlayerState(Parse(StatePrefix + TEXT("\"bad\"}")), State));
    const FString Counters = StatePrefix + TEXT("[{\"campaign_id\":\"c\",\"current_period_key\":\"p\",\"claimed_current_period\":false,\"total_claim_count\":1.5,\"next_reward_index\":0,\"reward_status\":\"available\"}]}");
    TestFalse(TEXT("领取计数必须为非负int32整数"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToPlayerState(Parse(Counters), State));
    TestTrue(TEXT("合法零计数仍成功"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToPlayerState(Parse(Counters.Replace(TEXT("1.5"), TEXT("0"))), State));

    const FString SeasonBody = Prefix + TEXT("\"catalog_version\":1,\"catalog_revision\":1,\"seasons\":[{\"season_id\":\"s\",\"name_key\":\"s\",\"version\":1,\"priority\":1.5,\"starts_at_utc\":\"2026-09-30T00:00:00Z\"}]}}");
    FGamePlatformLiveOpsCatalogSnapshot SeasonCatalog;
    TestFalse(TEXT("优先级不可截断小数"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(SeasonBody), SeasonCatalog));
    TestFalse(TEXT("优先级int32溢出拒绝"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(SeasonBody.Replace(TEXT("1.5"), TEXT("2147483648"))), SeasonCatalog));
    TestTrue(TEXT("优先级int32负边界合法"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(SeasonBody.Replace(TEXT("1.5"), TEXT("-2147483648"))), SeasonCatalog));
    FGamePlatformLiveOpsCatalogSnapshot InvalidElements;
    TestFalse(TEXT("目录数组非对象元素拒绝"), FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(Parse(Prefix + TEXT("\"catalog_version\":1,\"catalog_revision\":1,\"events\":[1]}}")), InvalidElements));
    return true;
}
#endif
