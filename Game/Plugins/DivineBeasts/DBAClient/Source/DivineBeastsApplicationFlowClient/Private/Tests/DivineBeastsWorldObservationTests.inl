#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
// 仅测试传输边界；不发HTTP/RPC，不把夹具事实当作真实联机准入。
class FReadinessReentryTransport final : public IGamePlatformSessionTransport
{
public:
    FGamePlatformSessionTransportCallbacks Callbacks;
    void BeginTransfer(const FGamePlatformSessionTransferRequest&, FGamePlatformSessionTransportCallbacks In) override
    { Callbacks = MoveTemp(In); }
    void CancelTransfer(const FGuid&) override {}
    void LeaveSession(const FGamePlatformSessionConnectionBinding&) override {}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldObservationTest,
    "DivineBeasts.ApplicationFlow.WorldObservation.CustomReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldObservationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    FURL URL;
    URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
    if (!TestTrue(TEXT("隔离World装配基础GameMode"), World->SetGameMode(URL))) return false;
    World->InitializeActorsForPlay(URL);
    World->BeginPlay();
    if (!TestTrue(TEXT("真实BeginPlay前置成立后才测试目标包分支"), World->HasBegunPlay())) return false;
    auto* Loading = IGamePlatformLoadingService::Get(*Instance);
    if (!TestNotNull(TEXT("真实实例Loading服务存在"), Loading)) return false;
    TStrongObjectPtr<UDivineBeastsApplicationFlowSubsystem> Flow(
        NewObject<UDivineBeastsApplicationFlowSubsystem>(Instance.Get()));
    Flow->Loading = Loading;
    Flow->ActiveTransferOperationId = FGuid::NewGuid();
    const FGuid Observation = Flow->ActiveTransferOperationId;
    const FName ExpectedWorld(TEXT("World.Test.Observation"));
    const FName ExpectedExperience(TEXT("Experience.Test.Observation"));
    Flow->LoadingContext = MakeShared<FDivineBeastsProjectLoadingContext>(
        *Instance, Observation, ExpectedWorld, ExpectedExperience);
    const TWeakPtr<FDivineBeastsProjectLoadingContext> WeakContext = Flow->LoadingContext;
    FGamePlatformResult Result;
    const FName TaskType(TEXT("Test.ProjectWorldObservation"));
    const auto Factory = Loading->RegisterTaskFactory(TaskType,
        [WeakContext]() -> TUniquePtr<IGamePlatformLoadingTask>
        { return MakeUnique<FDivineBeastsProjectReadinessTask>(WeakContext); }, Result);
    if (!TestTrue(TEXT("实际项目就绪任务工厂注册成功"), Result.IsSuccess())) return false;
    FGamePlatformLoadingOperationSpec Spec;
    Spec.Purpose = TEXT("Test.ProjectWorldObservation");
    FGamePlatformLoadingTaskSpec Task;
    Task.TaskId = TaskExpectedWorld; Task.TaskType = TaskType;
    Spec.Tasks.Add(Task);
    Flow->ActiveLoadingOperation = Loading->StartLoadingOperation(Spec, Flow.Get(), Result);
    const auto OriginalLoadingOperation = Flow->ActiveLoadingOperation;
    ON_SCOPE_EXIT
    {
        Loading->ReleaseLoadingOperation(OriginalLoadingOperation);
        Loading->UnregisterTaskFactory(Factory);
        Flow->ActiveLoadingOperation = {};
        Flow->LoadingContext.Reset();
        Flow->Loading = nullptr;
    };
    if (!TestTrue(TEXT("与正式入口相同的自定义任务操作已受理"), Result.IsSuccess())) return false;
    TestTrue(TEXT("项目任务不声明平台WorldPresence目标"), Spec.TargetWorldPackage.IsEmpty());
    TestFalse(TEXT("WorldDefinition尚未校验的早到通知不能完成世界观察"),
        Flow->NotifyWorldObserved(Observation, ExpectedExperience, ExpectedWorld, World));
    TestFalse(TEXT("早到通知不发布目标World已加载事实"),
        Flow->LoadingContext->Has(DivineBeastsLoading::Fact::ExpectedWorld));
    TestEqual(TEXT("等待定义期间不投影错误页"),
        Flow->GetViewState().Error, EDivineBeastsFlowError::None);
    // 此边界从已经校验过的项目WorldDefinition取得包身份；不伪造任何Session确认。
    Flow->LoadingContext->TargetWorldPackage = World->GetOutermost()->GetName();
    TestTrue(TEXT("本次真实World观察成功，不因未使用的内置目标为空而拒绝"),
        Flow->NotifyWorldObserved(Observation, ExpectedExperience, ExpectedWorld, World));
    TestTrue(TEXT("项目原生World与包身份检查仍成立"), Flow->LoadingContext->IsWorldOperable());
    TestEqual(TEXT("有效观察不会投影WorldMismatch错误页"),
        Flow->GetViewState().Error, EDivineBeastsFlowError::None);
    TestFalse(TEXT("未伪造服务器准入事实"),
        Flow->LoadingContext->Has(DivineBeastsLoading::Fact::SessionAdmission));
    TestFalse(TEXT("旧观察代次仍拒绝"),
        Flow->NotifyWorldObserved(FGuid::NewGuid(), ExpectedExperience, ExpectedWorld, World));
    TestFalse(TEXT("错误目标World身份仍拒绝"),
        Flow->NotifyWorldObserved(Observation, ExpectedExperience, TEXT("World.Wrong"), World));
    FTSTicker::GetCoreTicker().Tick(0.1f);
    TestTrue(TEXT("实际自定义任务经Loading服务收敛就绪"),
        Loading->IsReadyToPlay(Flow->ActiveLoadingOperation));

    // 模拟ControllerReady先到、World事实最后到；第一条本地报告同步进入Ready并释放流程。
    auto* Session = Instance->GetSubsystem<UGamePlatformSessionClientSubsystem>();
    if (!TestNotNull(TEXT("重入回归使用真实Session子系统"), Session)) return false;
    const auto Transport = MakeShared<FReadinessReentryTransport>();
    Session->SetTransport(Transport);
    Session->SetAuthenticationContext(TEXT("test-only-world-observation"), FGuid::NewGuid());
    FGamePlatformSessionConnectionBinding Binding;
    Binding.AssignmentId = TEXT("test-observation-assignment"); Binding.GameSessionId = TEXT("test-observation-session");
    Binding.ServerInstanceId = TEXT("test-observation-server"); Binding.ServerBootId = TEXT("test-observation-boot");
    Binding.WorldId = ExpectedWorld; Binding.ProtocolVersion = TEXT("1"); Binding.SessionEpoch = 1;
    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = Observation; Request.AssignmentId = Binding.AssignmentId;
    Request.GameServerId = Binding.ServerInstanceId; Request.ServerRoleId = TEXT("GameServer.Role.Village");
    Request.ExperienceId = ExpectedExperience; Request.WorldId = ExpectedWorld;
    Request.Endpoint = TEXT("127.0.0.1:7777"); Request.TransferTicket = TEXT("test-only-ticket");
    Request.TicketId = TEXT("test-observation-ticket"); Request.SessionId = TEXT("test-observation-session");
    Request.ExpectedBinding = Binding;
    if (!TestTrue(TEXT("离线传输前置已受理"), Session->BeginTransfer(Request, Result))) return false;
    Transport->Callbacks.OnBindingPrepared(Binding);
    Transport->Callbacks.OnTravelCommitted(Binding);
    Transport->Callbacks.OnFact(EGamePlatformSessionTransferFact::NetworkConnected, Binding);
    Transport->Callbacks.OnFact(EGamePlatformSessionTransferFact::AdmissionConfirmed, Binding);
    TestTrue(TEXT("ControllerReady先到前置成立"), Session->ReportLocalFact(
        Observation, EGamePlatformSessionTransferFact::ControllerReady, Binding, Result));
    Flow->Session = Session;
    Flow->LoadingContext->Observe(Observation, DivineBeastsLoading::Fact::CharacterBinding);
    bool ReleasedSynchronously = false;
    const auto Subscription = Session->OnSessionChanged().AddLambda([&](const FGamePlatformSessionSnapshot& Snapshot)
    {
        if (Snapshot.State == EGamePlatformSessionTransferState::Ready)
        {
            ReleasedSynchronously = true;
            Flow->ReleaseLoadingOperation();
        }
    });
    Flow->TryReportLocalFactsToSession();
    Session->OnSessionChanged().Remove(Subscription);
    Flow->Session = nullptr;
    TestTrue(TEXT("首条本地事实真实同步触发Ready清理"), ReleasedSynchronously);
    TestFalse(TEXT("同步释放后不解引用旧上下文或复活观察代次"),
        Flow->LoadingContext.IsValid() || Flow->ActiveTransferOperationId.IsValid());
    return true;
}
