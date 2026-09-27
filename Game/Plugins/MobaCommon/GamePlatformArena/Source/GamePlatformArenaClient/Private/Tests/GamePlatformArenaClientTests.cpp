#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Client/GamePlatformArenaClientTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaClientRequestTest,
    "GamePlatform.Arena.Client.MatchmakingRequestAuthorityBoundary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaClientRequestTest::RunTest(const FString& Parameters)
{
    FGamePlatformArenaMatchmakingRequest Request;
    Request.ArenaModeId = TEXT("Arena.Mode.Team5v5");
    Request.PartyId = TEXT("party-1");
    Request.PreferredRegion = TEXT("auto");
    Request.ClientRequestId = TEXT("req-1");
    FString Error;
    TestTrue(TEXT("允许字段请求合法"), Request.IsValid(Error));
    return true;
}

#endif
