#if WITH_DEV_AUTOMATION_TESTS

#include "GamePlatformSessionClientSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
class FTestGamePlatformSessionTransport final : public IGamePlatformSessionTransport
{
public:
    virtual void BeginTransfer(
        const FGamePlatformSessionTransferRequest& Request,
        FGamePlatformSessionTransportCallbacks InCallbacks) override
    {
        ++BeginCalls;
        LastRequest = Request;
        Callbacks = MoveTemp(InCallbacks);
    }

    virtual void CancelTransfer(const FGuid& TransferOperationId) override
    {
        ++CancelCalls;
        LastCancelledOperationId = TransferOperationId;
    }

    virtual void LeaveSession(
        const FGamePlatformSessionConnectionBinding& Binding) override
    {
        ++LeaveCalls;
        LastLeftBinding = Binding;
    }

    void CompleteReady(const FGamePlatformSessionConnectionBinding& Binding)
    {
        if (Callbacks.OnBindingPrepared)
        {
            Callbacks.OnBindingPrepared(Binding);
        }
        if (Callbacks.OnTravelCommitted)
        {
            Callbacks.OnTravelCommitted(Binding);
        }
        if (Callbacks.OnFact)
        {
            Callbacks.OnFact(
                EGamePlatformSessionTransferFact::NetworkConnected,
                Binding);
            Callbacks.OnFact(
                EGamePlatformSessionTransferFact::AdmissionConfirmed,
                Binding);
            Callbacks.OnFact(
                EGamePlatformSessionTransferFact::TargetWorldLoaded,
                Binding);
            Callbacks.OnFact(
                EGamePlatformSessionTransferFact::ControllerReady,
                Binding);
        }
    }

    int32 BeginCalls = 0;
    int32 CancelCalls = 0;
    int32 LeaveCalls = 0;
    FGuid LastCancelledOperationId;
    FGamePlatformSessionTransferRequest LastRequest;
    FGamePlatformSessionConnectionBinding LastLeftBinding;
    FGamePlatformSessionTransportCallbacks Callbacks;
};

FGamePlatformSessionTransferRequest MakeSubsystemRequest(
    const FGuid& OperationId,
    const TCHAR* AssignmentId = TEXT("assignment-session-test"))
{
    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = OperationId;
    Request.AssignmentId = AssignmentId;
    Request.GameServerId = TEXT("server-session-test");
    Request.ServerRoleId = TEXT("GameServer.Role.OpenWorld");
    Request.ExperienceId = TEXT("Experience.OpenWorld.Main");
    Request.WorldId = TEXT("World.OpenWorld.Main");
    Request.Endpoint = TEXT("127.0.0.1:7777");
    Request.TransferTicket = TEXT("test-only-transfer-ticket");
    Request.TicketId = TEXT("ticket-session-test");
    Request.SessionId = TEXT("session-session-test");
    Request.TimeoutSeconds = 30.0;
    return Request;
}

FGamePlatformSessionConnectionBinding MakeSubsystemBinding(
    int64 SessionEpoch = 1)
{
    FGamePlatformSessionConnectionBinding Binding;
    Binding.AssignmentId = TEXT("assignment-session-test");
    Binding.GameSessionId = TEXT("game-session-test");
    Binding.ServerInstanceId = TEXT("server-session-test");
    Binding.ServerBootId = TEXT("boot-session-test");
    Binding.WorldId = TEXT("World.OpenWorld.Main");
    Binding.ProtocolVersion = TEXT("1");
    Binding.SessionEpoch = SessionEpoch;
    return Binding;
}

/**
 * FGamePlatformSessionSubsystemIntegrationCommand（Session子系统集成测试命令）。
 *
 * 使用真实UGameInstance创建生产子系统，只替换外部Transport边界；
 * 不伪造UE网络成功，只验证公开适配层的幂等、取消、旧回调栅栏与登出离开行为。
 */
class FGamePlatformSessionSubsystemIntegrationCommand final
    : public IAutomationLatentCommand
{
public:
    explicit FGamePlatformSessionSubsystemIntegrationCommand(
        FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual ~FGamePlatformSessionSubsystemIntegrationCommand() override
    {
        Cleanup();
    }

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 20.0)
        {
            Test->AddError(TEXT("Session子系统集成测试超时。"));
            Cleanup();
            return true;
        }

        switch (Phase)
        {
        case 0:
            return StartAndVerifyIdempotency();
        case 1:
            return VerifyTransportReplacementAndLateCallbacks();
        case 2:
            return StartReadySession();
        case 3:
            return VerifyReadyAndLogout();
        default:
            Cleanup();
            return true;
        }
    }

private:
    bool StartAndVerifyIdempotency()
    {
        if (!GEngine)
        {
            Test->AddError(TEXT("Session子系统测试需要已初始化的UE引擎。"));
            return true;
        }

        Instance.Reset(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone(
            FName(*(
                TEXT("SessionTest_") +
                FGuid::NewGuid().ToString(EGuidFormats::Digits))));

        Session = Instance->GetSubsystem<UGamePlatformSessionClientSubsystem>();
        Test->TestNotNull(TEXT("真实GameInstance创建Session子系统"), Session);
        if (!Session)
        {
            Cleanup();
            return true;
        }

        TransportA = MakeShared<FTestGamePlatformSessionTransport>();
        Session->SetTransport(TransportA);
        Session->SetAuthenticationContext(TEXT("account-session-test"), FGuid::NewGuid());

        FirstOperation = FGuid::NewGuid();
        FGamePlatformSessionTransferRequest Request =
            MakeSubsystemRequest(FirstOperation);
        FGamePlatformResult Result;
        Test->TestTrue(
            TEXT("首次Join操作被接纳"),
            Session->BeginOperation(
                EGamePlatformSessionIntent::Join,
                Request,
                Result));
        Test->TestTrue(TEXT("首次Join结果成功"), Result.IsSuccess());

        Result = {};
        Test->TestTrue(
            TEXT("同Operation活动重入按幂等成功返回"),
            Session->BeginOperation(
                EGamePlatformSessionIntent::Join,
                Request,
                Result));
        Test->TestEqual(
            TEXT("幂等重入不得重复调用Transport"),
            TransportA->BeginCalls,
            1);

        // 活动过程中替换Transport必须先取消旧Transport，避免旧网络任务继续运行。
        TransportB = MakeShared<FTestGamePlatformSessionTransport>();
        Session->SetTransport(TransportB);
        Test->TestEqual(
            TEXT("替换Transport会取消旧活动操作"),
            TransportA->CancelCalls,
            1);
        Test->TestEqual(
            TEXT("旧Transport收到正确OperationId"),
            TransportA->LastCancelledOperationId,
            FirstOperation);

        // 在取消后故意触发旧Transport保存的迟到回调；Subsystem应通过Operation栅栏全部忽略。
        const FGamePlatformSessionConnectionBinding LateBinding =
            MakeSubsystemBinding();
        TransportA->CompleteReady(LateBinding);
        ++Phase;
        return false;
    }

    bool VerifyTransportReplacementAndLateCallbacks()
    {
        const FGamePlatformSessionSnapshot Snapshot = Session->GetSnapshot();
        Test->TestTrue(
            TEXT("旧Transport迟到回调不能恢复Ready"),
            Snapshot.State != EGamePlatformSessionTransferState::Ready);
        Test->TestEqual(
            TEXT("新Transport没有被旧操作隐式调用"),
            TransportB->BeginCalls,
            0);

        // 取消后的远端状态需要显式对账。确认无远端绑定后才允许发起新的Join。
        FGamePlatformResult Result;
        Test->TestTrue(
            TEXT("取消后允许显式确认远端无绑定"),
            Session->ResolveRemoteState(
                FirstOperation,
                FGamePlatformSessionConnectionBinding(),
                Result));
        Test->TestTrue(
            TEXT("对账后允许重新尝试"),
            Session->GetSnapshot().bCanRetry);

        ++Phase;
        return false;
    }

    bool StartReadySession()
    {
        ReadyOperation = FGuid::NewGuid();
        FGamePlatformSessionTransferRequest Request =
            MakeSubsystemRequest(ReadyOperation);
        FGamePlatformResult Result;
        Test->TestTrue(
            TEXT("对账后新的Join被新Transport接纳"),
            Session->BeginOperation(
                EGamePlatformSessionIntent::Join,
                Request,
                Result));
        Test->TestEqual(
            TEXT("新Transport只接收一次新操作"),
            TransportB->BeginCalls,
            1);

        TransportB->CompleteReady(MakeSubsystemBinding(2));
        ++Phase;
        return false;
    }

    bool VerifyReadyAndLogout()
    {
        const FGamePlatformSessionSnapshot BeforeLogout =
            Session->GetSnapshot();
        Test->TestEqual(
            TEXT("四个可信事实完成后Session进入Ready"),
            BeforeLogout.State,
            EGamePlatformSessionTransferState::Ready);
        Test->TestTrue(
            TEXT("Ready快照具有有效Binding"),
            BeforeLogout.Binding.IsValid());

        Session->SetAuthenticationContext(FString(), FGuid::NewGuid());
        Test->TestEqual(
            TEXT("Ready状态退出认证会调用Transport离开旧会话"),
            TransportB->LeaveCalls,
            1);
        Test->TestTrue(
            TEXT("离开时向Transport传递原可信Binding"),
            TransportB->LastLeftBinding.IsValid());

        const FGamePlatformSessionSnapshot AfterLogout =
            Session->GetSnapshot();
        Test->TestTrue(
            TEXT("登出后不再保持Ready"),
            AfterLogout.State != EGamePlatformSessionTransferState::Ready);
        Test->TestFalse(
            TEXT("登出后公开Binding被清理"),
            AfterLogout.Binding.IsValid());

        ++Phase;
        return false;
    }

    void Cleanup()
    {
        if (!Instance.IsValid())
        {
            return;
        }

        UWorld* World = Instance->GetWorld();
        if (World)
        {
            World->DestroyWorld(false);
        }
        Instance->Shutdown();
        if (World && GEngine)
        {
            GEngine->DestroyWorldContext(World);
        }

        Session = nullptr;
        TransportA.Reset();
        TransportB.Reset();
        Instance.Reset();
    }

    FAutomationTestBase* Test = nullptr;
    double StartedSeconds = FPlatformTime::Seconds();
    int32 Phase = 0;

    TStrongObjectPtr<UGameInstance> Instance;
    UGamePlatformSessionClientSubsystem* Session = nullptr;
    TSharedPtr<FTestGamePlatformSessionTransport> TransportA;
    TSharedPtr<FTestGamePlatformSessionTransport> TransportB;
    FGuid FirstOperation;
    FGuid ReadyOperation;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSessionSubsystemIntegrationTest,
    "GamePlatform.Session.Subsystem.TransportRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSessionSubsystemIntegrationTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        FGamePlatformSessionSubsystemIntegrationCommand(this));
    return true;
}

#endif
