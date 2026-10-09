#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Types/GamePlatformSessionErrors.h"

namespace
{
    /**
     * 仅测试使用的合法 GameInstance 作用域：Session 继承的 ClassWithin 要求
     * GameInstance Outer，即使只验证非法枚举也不能使用 Package。显式构造并强持有
     * 实例和子系统，不 Init 游戏实例、不初始化自动依赖、不创建 World/网络连接。
     * 所有退出先 Deinitialize 清理请求/委托/Ticker，再释放子系统及宿主。
     */
    struct FSessionGameInstanceFixture
    {
        TStrongObjectPtr<UGameInstance> Instance;
        TStrongObjectPtr<UGamePlatformSessionClientSubsystem> Subsystem;

        bool Initialize(FAutomationTestBase& Test)
        {
            Instance.Reset(NewObject<UGameInstance>());
            if (!Test.TestNotNull(TEXT("会话夹具需要合法GameInstance宿主"), Instance.Get()))
            {
                return false;
            }
            Subsystem.Reset(NewObject<UGamePlatformSessionClientSubsystem>(Instance.Get()));
            return Test.TestNotNull(TEXT("会话子系统具有合法GameInstance Outer"), Subsystem.Get());
        }

        ~FSessionGameInstanceFixture()
        {
            if (Subsystem.IsValid())
            {
                Subsystem->Deinitialize();
            }
            Subsystem.Reset();
            Instance.Reset();
        }
    };

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
    Request.ExpectedBinding.AssignmentId = Request.AssignmentId;
    Request.ExpectedBinding.GameSessionId = TEXT("game-session-test");
    Request.ExpectedBinding.ServerInstanceId = Request.GameServerId;
    Request.ExpectedBinding.ServerBootId = TEXT("boot-test");
    Request.ExpectedBinding.WorldId = Request.WorldId;
    Request.ExpectedBinding.ProtocolVersion = TEXT("1");
    Request.ExpectedBinding.SessionEpoch = 1;
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
    FSessionGameInstanceFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformSessionClientSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
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
