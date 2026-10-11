// 项目注销回归：真实Session子系统+仅测试Transport，覆盖传输取消/Ready离开/旧事实失效；不伪造UE联网验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 测试替身仅记录物理边界调用，可信事实由测试明确发送，不进入生产配置。 */
class FLogoutTestTransport final : public IGamePlatformSessionTransport
{
public:
    FGamePlatformSessionTransportCallbacks Callbacks;
    int32 CancelCalls = 0;
    int32 LeaveCalls = 0;
    virtual void BeginTransfer(const FGamePlatformSessionTransferRequest&, FGamePlatformSessionTransportCallbacks InCallbacks) override { Callbacks = MoveTemp(InCallbacks); }
    virtual void CancelTransfer(const FGuid&) override { ++CancelCalls; }
    virtual void LeaveSession(const FGamePlatformSessionConnectionBinding&) override { ++LeaveCalls; }
    void EmitReady(const FGamePlatformSessionConnectionBinding& Binding)
    {
        if (Callbacks.OnBindingPrepared) Callbacks.OnBindingPrepared(Binding);
        if (Callbacks.OnTravelCommitted) Callbacks.OnTravelCommitted(Binding);
        if (Callbacks.OnFact)
            for (const auto Fact : {EGamePlatformSessionTransferFact::NetworkConnected, EGamePlatformSessionTransferFact::AdmissionConfirmed,
                EGamePlatformSessionTransferFact::TargetWorldLoaded, EGamePlatformSessionTransferFact::ControllerReady}) Callbacks.OnFact(Fact, Binding);
    }
};
}

/** 独立GameInstance所有权；各Update间隔允许Session游戏线程队列处理可信事实。无账号/HTTP/真实服务器。 */
class FDivineBeastsLogoutSessionLifetimeCommand final : public IAutomationLatentCommand
{
public:
    explicit FDivineBeastsLogoutSessionLifetimeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual ~FDivineBeastsLogoutSessionLifetimeCommand() override { Cleanup(); }
    virtual bool Update() override
    {
        if (Phase == 0)
        {
            if (!GEngine) { Test->AddError(TEXT("注销回归需要UE引擎。")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*(TEXT("LogoutTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            Session = Instance->GetSubsystem<UGamePlatformSessionClientSubsystem>();
            if (!Session) { Test->AddError(TEXT("真实Session子系统不可用。")); Cleanup(); return true; }
            // Flow对象不初始化Online/Backend：精确验证新增Session注销路径，不尝试认证或加载正式业务资产。
            Flow.Reset(NewObject<UDivineBeastsApplicationFlowSubsystem>(Instance.Get()));
            Flow->Session = Session;
            Transport = MakeShared<FLogoutTestTransport>();
            Session->SetTransport(Transport);
            Session->SetAuthenticationContext(TEXT("test-only-logout-account"), FGuid::NewGuid());
            Binding.AssignmentId = TEXT("logout-assignment"); Binding.GameSessionId = TEXT("logout-game-session");
            Binding.ServerInstanceId = TEXT("logout-server"); Binding.ServerBootId = TEXT("logout-boot");
            Binding.WorldId = TEXT("World.OpenWorld.Main"); Binding.ProtocolVersion = TEXT("1"); Binding.SessionEpoch = 1;
            Request.AssignmentId = Binding.AssignmentId; Request.GameServerId = Binding.ServerInstanceId;
            Request.ServerRoleId = TEXT("GameServer.Role.OpenWorld"); Request.ExperienceId = TEXT("Experience.OpenWorld.Main");
            Request.WorldId = Binding.WorldId; Request.Endpoint = TEXT("127.0.0.1:7777");
            Request.TransferTicket = TEXT("test-only-ticket"); Request.TicketId = TEXT("logout-ticket");
            Request.SessionId = TEXT("logout-session"); Request.ExpectedBinding = Binding; Request.TimeoutSeconds = 30.0;
            Request.TransferOperationId = FGuid::NewGuid();
            FGamePlatformResult Result;
            if (!Session->BeginTransfer(Request, Result)) { Test->AddError(TEXT("测试传输前提失败：") + Result.Message); Cleanup(); return true; }
            Flow->LogoutAndRestart();
            Test->TestEqual(TEXT("未完成传输注销调用一次Cancel"), Transport->CancelCalls, 1);
            // 内核连接State虽已Idle，公开Snapshot.State保留最近操作LastOutcome=Cancelled，不能混为一谈。
            // 核验取消终态、无准入/绑定、无远端恢复屏障及旧代次拒绝，而不是仅修改枚举让测试通过。
            const auto CancelledSnapshot = Session->GetSnapshot();
            Test->TestEqual(TEXT("未完成传输注销保留真实Cancelled操作终态"), CancelledSnapshot.State, EGamePlatformSessionTransferState::Cancelled);
            Test->TestEqual(TEXT("最近操作ID仅供终态诊断保留"), CancelledSnapshot.TransferOperationId, Request.TransferOperationId);
            Test->TestFalse(TEXT("传输注销没有残留可信Binding"), CancelledSnapshot.Binding.IsValid());
            Test->TestFalse(TEXT("传输注销撤销准入事实"), CancelledSnapshot.bAdmissionConfirmed);
            Test->TestFalse(TEXT("尚未分配或提交Travel的取消不伪造远端待恢复状态"), CancelledSnapshot.bRecoveryRequired);
            // 本夹具刻意没有Online，只验证Session离开；测试认证仍在，取消后下一操作必须确实可受理。
            Test->TestTrue(TEXT("未越过远端边界的取消允许后续操作"), CancelledSnapshot.bCanRetry);
            Test->TestFalse(TEXT("注销后的旧本地就绪事实不可复活传输"), Session->ReportLocalFact(
                Request.TransferOperationId, EGamePlatformSessionTransferFact::TargetWorldLoaded, Binding, Result));
            Request.TransferOperationId = FGuid::NewGuid();
            if (!Session->BeginTransfer(Request, Result)) { Test->AddError(TEXT("Ready测试前提失败：") + Result.Message); Cleanup(); return true; }
            Transport->EmitReady(Binding);
            ++Phase; return false;
        }
        if (Phase == 1)
        {
            Test->TestEqual(TEXT("可信四事实建立Ready前提"), Session->GetSnapshot().State, EGamePlatformSessionTransferState::Ready);
            Flow->LogoutAndRestart();
            Test->TestEqual(TEXT("Ready注销真实调用Leave而不是只Cancel"), Transport->LeaveCalls, 1);
            Test->TestFalse(TEXT("已确认世界Binding立即撤销"), Session->GetSnapshot().Binding.IsValid());
            Transport->EmitReady(Binding); // 模拟网络队列里的旧事实，不能复活注销连接。
            ++Phase; return false;
        }
        Test->TestFalse(TEXT("注销后迟到事实不能重新Ready"), Session->GetSnapshot().State == EGamePlatformSessionTransferState::Ready);
        Test->TestFalse(TEXT("迟到事实不能恢复旧Binding"), Session->GetSnapshot().Binding.IsValid());
        Flow->LogoutAndRestart();
        Test->TestEqual(TEXT("重复注销不会再离开同一连接"), Transport->LeaveCalls, 1);
        Cleanup(); return true;
    }
private:
    void Cleanup()
    {
        if (!Instance.IsValid()) return;
        Flow.Reset();
        UWorld* World = Instance->GetWorld();
        if (World) World->DestroyWorld(false);
        Instance->Shutdown();
        if (World && GEngine) GEngine->DestroyWorldContext(World);
        Session = nullptr; Transport.Reset(); Instance.Reset();
    }
    FAutomationTestBase* Test;
    int32 Phase = 0;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UDivineBeastsApplicationFlowSubsystem> Flow;
    UGamePlatformSessionClientSubsystem* Session = nullptr;
    TSharedPtr<FLogoutTestTransport> Transport;
    FGamePlatformSessionTransferRequest Request;
    FGamePlatformSessionConnectionBinding Binding;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLogoutSessionLifetimeTest, "DivineBeasts.ApplicationFlow.Logout.SessionLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsLogoutSessionLifetimeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FDivineBeastsLogoutSessionLifetimeCommand(this));
    return true;
}
#endif
