// 真实JSON解析回归：仅测试夹具，不访问网络；调用生产HTTP适配的私有解析器，验证畸形输入不能发布成功空快照。
#include "Transport/GamePlatformProgressionGatewayHttpTransport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProgressionJsonValidationTest, "GamePlatform.Progression.Client.JsonValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProgressionJsonValidationTest::RunTest(const FString&)
{
    auto Parse = [](const FString& Body) { TSharedPtr<FJsonObject> Json; auto Reader = TJsonReaderFactory<>::Create(Body); FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings); return Json; };

    int64 Value = 0;
    TestFalse(TEXT("十进制字符串正溢出拒绝"), FGamePlatformProgressionGatewayHttpTransport::ParseInt64String(Parse(TEXT("{\"xp\":\"9223372036854775808\"}")), TEXT("xp"), Value));
    TestFalse(TEXT("十进制字符串负溢出拒绝"), FGamePlatformProgressionGatewayHttpTransport::ParseInt64String(Parse(TEXT("{\"xp\":\"-9223372036854775809\"}")), TEXT("xp"), Value));
    TestTrue(TEXT("合法int64上界保留"), FGamePlatformProgressionGatewayHttpTransport::ParseInt64String(Parse(TEXT("{\"xp\":\"9223372036854775807\"}")), TEXT("xp"), Value));
    TestEqual(TEXT("int64值保真"), Value, int64(MAX_int64));
    return true;
}
#endif
