#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Types/GamePlatformSessionErrors.h"

namespace
{
FGamePlatformSessionTransferRequest MakeValidRequest()
{
    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = FGuid::NewGuid();
    Request.AssignmentId = TEXT("assignment-test");
    Request.GameServerId = TEXT("server-test");
    Request.ServerRoleId = TEXT("GameServer.Role.OpenWorld");
    Request.ExperienceId = TEXT("Experience.OpenWorld.Main");
    Request.WorldId = TEXT("World.OpenWorld.Main");
    Request.Endpoint = TEXT("127.0.0.1:7777");
    Request.TransferTicket = TEXT("test-only-sensitive-ticket");
    Request.TicketId = TEXT("ticket-test");
    Request.SessionId = TEXT("session-test");
    Request.TimeoutSeconds = 30.0;
    return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSessionTransferRequestValidationTest,
    "GamePlatform.Session.TransferRequest.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSessionTransferRequestValidationTest::RunTest(const FString&)
{
    FGamePlatformSessionTransferRequest Request = MakeValidRequest();
    TestTrue(TEXT("平台请求不强制具体游戏CharacterId"), Request.Validate().IsSuccess());

    Request.Endpoint = TEXT("127.0.0.1:7777?TransferTicket=secret");
    FGamePlatformResult Result = Request.Validate();
    TestFalse(TEXT("Endpoint禁止URL参数"), Result.IsSuccess());
    TestEqual(TEXT("Endpoint返回稳定错误码"), Result.Code, GamePlatformSessionErrors::TransferEndpointInvalid);

    Request = MakeValidRequest();
    Request.Endpoint = TEXT("127.0.0.1:7777\nInjected: value");
    Result = Request.Validate();
    TestFalse(TEXT("Endpoint禁止换行注入"), Result.IsSuccess());

    Request = MakeValidRequest();
    Request.TimeoutSeconds = 0.5;
    Result = Request.Validate();
    TestFalse(TEXT("过短超时预算被拒绝"), Result.IsSuccess());
    TestEqual(TEXT("超时返回稳定错误码"), Result.Code, GamePlatformSessionErrors::TransferTimeoutInvalid);

    Request = MakeValidRequest();
    Request.TransferTicket.Reset();
    Result = Request.Validate();
    TestFalse(TEXT("缺失一次性票据必须拒绝"), Result.IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSessionInvalidFactTest,
    "GamePlatform.Session.Fact.InvalidEnumRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSessionInvalidFactTest::RunTest(const FString&)
{
    UGamePlatformSessionClientSubsystem* Subsystem =
        NewObject<UGamePlatformSessionClientSubsystem>(GetTransientPackage());
    TestNotNull(TEXT("可创建测试Session子系统对象"), Subsystem);
    if (!Subsystem)
    {
        return false;
    }

    FGamePlatformResult Result;
    const bool bAccepted = Subsystem->ReportLocalFact(
        FGuid::NewGuid(),
        static_cast<EGamePlatformSessionTransferFact>(255),
        FGamePlatformSessionConnectionBinding(),
        Result);
    TestFalse(TEXT("非法Fact绝不推进状态"), bAccepted);
    TestEqual(TEXT("非法Fact返回稳定错误码"), Result.Code, GamePlatformSessionErrors::FactInvalid);
    return true;
}

#endif
