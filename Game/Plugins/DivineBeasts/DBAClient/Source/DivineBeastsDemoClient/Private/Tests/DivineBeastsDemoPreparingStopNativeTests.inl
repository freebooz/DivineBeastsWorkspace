// 由WorldRun.cpp仅在WITH_DEV_AUTOMATION_TESTS包含；真实World/Catalog/Data/BeginScenario链回归。
// CoreIdentity来自实际Catalog；仅测试输入副本增加引擎Cube普通资源，原Catalog/资产不写入。
// 转发包装器只计数并可注入一次清理失败；不替代中央执行器、数据加载或真实Core适配器。
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FPreparingStopProbe
{
    int32 Prepares = 0, Stops = 0, Destructions = 0;
    bool bFailFirstStop = false;
    FDivineBeastsDemoRunToken Token;
    TFunction<void()> OnPrepared;
};
class FPreparingStopForwarder final : public IDivineBeastsDemoScenarioAdapter
{
    TSharedPtr<IDivineBeastsDemoScenarioAdapter> Inner;
    TSharedRef<FPreparingStopProbe> Probe;
public:
    FPreparingStopForwarder(TSharedPtr<IDivineBeastsDemoScenarioAdapter> In, TSharedRef<FPreparingStopProbe> InProbe)
        : Inner(MoveTemp(In)), Probe(MoveTemp(InProbe)) {}
    ~FPreparingStopForwarder() override { ++Probe->Destructions; }
    void Prepare(const FDivineBeastsDemoScenarioContext& C, FDivineBeastsDemoOperationCompletion Done) override
    {
        ++Probe->Prepares; Probe->Token = C.Token;
        Inner->Prepare(C, [P = Probe, Done = MoveTemp(Done)](const auto& Token, const auto& Result) mutable
        {
            // 原适配器已接收Context，中央Prepare调用栈仍未返回；真实回调内重入Stop。
            if (P->OnPrepared) P->OnPrepared();
            Done(Token, Result);
        });
    }
    void Execute(FName Op, const TArray<FDivineBeastsDemoParameterValue>& Values, FDivineBeastsDemoOperationCompletion Done) override
    { Inner->Execute(Op, Values, MoveTemp(Done)); }
    void Stop(FDivineBeastsDemoOperationCompletion Done) override
    {
        ++Probe->Stops;
        if (Probe->bFailFirstStop && Probe->Stops == 1)
        { Done(Probe->Token, FGamePlatformResult::Failure(TEXT("Test.PreparingStop.Cleanup"), TEXT("测试注入一次已进入Prepare的清理失败。"))); return; }
        Inner->Stop(MoveTemp(Done));
    }
};
struct FPreparingStopNativeFixture
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> Runner;
    TWeakObjectPtr<UGameInstance> Instance;
    IGamePlatformDataService* Data = nullptr;
    FDivineBeastsDemoScenarioDefinition Scenario;
    FSoftObjectPath Path{TEXT("/Engine/BasicShapes/Cube.Cube")};
    FGamePlatformDataLease External, OldLease, SuccessorLease;
    FDivineBeastsDemoRunToken OldToken, SuccessorToken;
    TSharedRef<FPreparingStopProbe> First = MakeShared<FPreparingStopProbe>();
    TSharedRef<FPreparingStopProbe> Next = MakeShared<FPreparingStopProbe>();
    FDelegateHandle Subscription;
    bool bExternalDone = false, bExternalSuccess = false, bDestroyed = false, bFirstNotification = true;
    int32 Phase = 0;
    double Deadline = FPlatformTime::Seconds() + 35.0;
    void Destroy()
    {
        if (bDestroyed || !World.IsValid()) return;
        bDestroyed = true;
        if (Runner.IsValid()) Runner->OnRunChanged.Remove(Subscription);
        // 真实UWorld清理调用Subsystem.Deinitialize->ShutdownScenario；不直接改bClosing或伪造终态。
        World->DestroyWorld(false);
        if (GEngine) GEngine->DestroyWorldContext(World.Get());
    }
    ~FPreparingStopNativeFixture()
    {
        Destroy();
        if (Instance.IsValid() && Data && External.IsValid()) Data->ReleaseResources(External);
    }
};
class FPreparingStopNativeTick final : public IAutomationLatentCommand
{
    TFunction<bool()> Step;
public:
    explicit FPreparingStopNativeTick(TFunction<bool()> In) : Step(MoveTemp(In)) {}
    bool Update() override { return Step(); }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDivineBeastsDemoPreparingStopNativeTest,
    "DivineBeasts.Demo.WorldLive.PreparingStop", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
void FDivineBeastsDemoPreparingStopNativeTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const TCHAR* Case : {TEXT("EventSuccessor"), TEXT("PendingResourceSuccessor"), TEXT("EnteredPrepareRetry"), TEXT("WorldExitPending"), TEXT("WorldExitEntered")})
    { Names.Add(Case); Commands.Add(Case); }
}
bool FDivineBeastsDemoPreparingStopNativeTest::RunTest(const FString& Case)
{
    const auto F = MakeShared<FPreparingStopNativeFixture>();
    UDivineBeastsDemoCatalogSubsystem* Catalog = nullptr;
    if (GEngine) for (const auto& C : GEngine->GetWorldContexts())
    {
        auto* W = C.World(); auto* GI = W ? W->GetGameInstance() : nullptr;
        if (!W || !W->IsGameWorld() || W->GetNetMode() != NM_Standalone || !GI) continue;
        auto* Candidate = GI->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();
        if (Candidate && Candidate->FindScenarioDefinition(TEXT("GamePlatformCore.Identity.Normal"), F->Scenario))
        { F->Instance = GI; Catalog = Candidate; break; }
    }
    if (!TestNotNull(TEXT("需要真实Standalone宿主已发布Catalog"), Catalog)) return false;
    if (!TestTrue(TEXT("原Core案例真实预检"), Catalog->PreflightScenario(F->Scenario.ScenarioId).bCanStart)) return false;
    F->Data = IGamePlatformDataService::Get(*F->Instance.Get());
    if (!TestNotNull(TEXT("真实中央Data"), F->Data) || !TestTrue(TEXT("引擎原生Cube包存在"), FPackageName::DoesPackageExist(F->Path.GetLongPackageName()))) return false;
    // 仅运行输入副本：保持Core身份/参数/适配器，增加真实执行器资源申请窗口。
    F->Scenario.WidgetAsset.Reset(); F->Scenario.DefinitionAssets = {F->Path};
    F->Scenario.Preconditions.Add(TEXT("原生中央执行器生命周期夹具：真实Cube资源；非Core业务验收。"));
    F->World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
    if (!TestNotNull(TEXT("独立原生World"), F->World.Get())) return false;
    F->World->SetGameInstance(F->Instance.Get());
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(F->World.Get());
    F->Runner.Reset(F->World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>());
    if (!TestNotNull(TEXT("真实WorldSubsystem集合中的执行器"), F->Runner.Get())) return false;
    const TWeakPtr<FPreparingStopNativeFixture> Weak = F;
    FGamePlatformResult Accepted;
    F->External = F->Data->AcquireResources({F->Path}, EGamePlatformDataLifetime::Instance, F->Instance.Get(),
        [Weak](const auto&, const auto& R) { if (auto S = Weak.Pin()) { S->bExternalDone = true; S->bExternalSuccess = R.IsSuccess(); } }, Accepted);
    if (!TestTrue(TEXT("独立外部完整租约受理"), Accepted.IsSuccess() && F->External.IsValid())) return false;
    if (Case == TEXT("EnteredPrepareRetry"))
    {
        F->First->bFailFirstStop = true;
        F->First->OnPrepared = [this, Weak]
        {
            if (auto S = Weak.Pin())
            {
                TestTrue(TEXT("进入Prepare标记先于外部回调"), S->Runner->RunPrivateState->bAdapterPrepareEntered);
                TestTrue(TEXT("Prepare回调栈内Stop受理"), S->Runner->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("原生故障回归"))));
            }
        };
    }
    F->Subscription = F->Runner->OnRunChanged.AddLambda([this, Weak, Case](const FDivineBeastsDemoRunSnapshot& Snapshot)
    {
        auto S = Weak.Pin(); if (!S || Snapshot.State != EDivineBeastsDemoRunState::Preparing) return;
        const bool bFirst = S->bFirstNotification; S->bFirstNotification = false;
        auto& Run = *S->Runner->RunPrivateState;
        TestFalse(TEXT("Preparing首通知尚未进入Adapter.Prepare"), Run.bAdapterPrepareEntered);
        const auto Probe = bFirst ? S->First : S->Next;
        Run.Adapter = MakeShared<FPreparingStopForwarder>(Run.Adapter, Probe);
        if (bFirst && Case == TEXT("EventSuccessor"))
        {
            S->OldToken = Snapshot.Token;
            TestFalse(TEXT("Preparing通知先于本Run资源申请"), Run.ResourceLease.IsValid());
            TestTrue(TEXT("Preparing事件中真实Stop"), S->Runner->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("原生事件取消"))));
            TestFalse(TEXT("未Prepare Stop同步释放运行"), S->Runner->HasActiveScenario());
            TestEqual(TEXT("未Prepare不能调用Adapter.Stop"), S->First->Stops, 0);
            TestTrue(TEXT("同一通知栈启动后继"), S->Runner->BeginScenario(S->Scenario));
            S->SuccessorToken = S->Runner->GetRunSnapshot().Token;
        }
    });
    ADD_LATENT_AUTOMATION_COMMAND(FPreparingStopNativeTick([this, F, Case]() -> bool
    {
        if (FPlatformTime::Seconds() > F->Deadline) { AddError(TEXT("真实PreparingStop原生回归超时")); return true; }
        if (!F->bExternalDone) return false;
        if (!F->bExternalSuccess) { AddError(TEXT("外部原生资源加载失败")); return true; }
        auto* R = F->Runner.Get();
        if (F->Phase == 0)
        {
            const auto Before = F->Data->GetDiagnostics().TotalAcceptedRequests;
            if (!TestTrue(TEXT("真实BeginScenario"), R->BeginScenario(F->Scenario))) return true;
            F->Phase = 1;
            if (Case == TEXT("EventSuccessor"))
            {
                TestEqual(TEXT("只有后继签发资源，旧Begin返回不得继续申请"), F->Data->GetDiagnostics().TotalAcceptedRequests, Before + 1);
                TestTrue(TEXT("旧Begin返回保留后继Token"), R->GetRunSnapshot().Token == F->SuccessorToken && !(F->SuccessorToken == F->OldToken));
                F->SuccessorLease = R->RunPrivateState->ResourceLease;
            }
            else
            {
                F->OldToken = R->GetRunSnapshot().Token; F->OldLease = R->RunPrivateState->ResourceLease;
                TestTrue(TEXT("真实中央资源租约已签发但Prepare未进入"), F->OldLease.IsValid() && !R->RunPrivateState->bAdapterPrepareEntered && F->First->Prepares == 0);
                TestTrue(TEXT("缓存驻留也必须延后完成"), F->Data->GetLeaseState(F->OldLease) == EGamePlatformDataRequestState::Loading);
                if (Case == TEXT("PendingResourceSuccessor"))
                {
                    TestTrue(TEXT("资源加载中Stop"), R->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("原生资源取消"))));
                    TestFalse(TEXT("资源取消同步收口"), R->HasActiveScenario());
                    TestTrue(TEXT("取消只撤本Run租约"), F->Data->GetLeaseState(F->OldLease) == EGamePlatformDataRequestState::Released);
                    TestTrue(TEXT("加载取消后启动后继"), R->BeginScenario(F->Scenario));
                    F->SuccessorToken = R->GetRunSnapshot().Token; F->SuccessorLease = R->RunPrivateState->ResourceLease;
                }
                else if (Case == TEXT("WorldExitPending")) F->Destroy();
            }
            return false;
        }
        if (Case == TEXT("EventSuccessor") || Case == TEXT("PendingResourceSuccessor"))
        {
            const auto Snapshot = R->GetRunSnapshot();
            if (Snapshot.State == EDivineBeastsDemoRunState::Preparing) return false;
            if (!TestTrue(TEXT("后继真实Prepare成功"), Snapshot.State == EDivineBeastsDemoRunState::Running)) return true;
            TestEqual(TEXT("旧适配器未Prepare"), F->First->Prepares, 0); TestEqual(TEXT("旧适配器未Stop"), F->First->Stops, 0);
            TestEqual(TEXT("后继Prepare仅一次"), F->Next->Prepares, 1);
            if (F->OldLease.IsValid())
            {
                // 真取消完成已由Data延后投递；另重放同一真实旧身份的迟到成功/取消，验证中央防御。
                R->OnScenarioResourcesReady(F->OldToken, F->OldLease, FGamePlatformResult::Success());
                R->OnScenarioResourcesReady(F->OldToken, F->OldLease, FGamePlatformResult::Cancelled(TEXT("测试重复迟到")));
            }
            TestTrue(TEXT("迟到旧资源回调不改后继Token/完整租约"), R->GetRunSnapshot().Token == F->SuccessorToken && IsSameScenarioLease(R->RunPrivateState->ResourceLease, F->SuccessorLease));
            TestTrue(TEXT("后继及外部资源仍成功"), F->Data->GetLeaseState(F->SuccessorLease) == EGamePlatformDataRequestState::Succeeded && F->Data->GetLeaseState(F->External) == EGamePlatformDataRequestState::Succeeded);
            TestTrue(TEXT("后继真实Stop"), R->RequestScenarioStop(FGamePlatformResult::Success()));
            TestFalse(TEXT("后继清理完成"), R->HasActiveScenario());
            TestEqual(TEXT("已Prepare后继必须调用Stop"), F->Next->Stops, 1);
            TestFalse(TEXT("终态前清除本Run阶段"), R->RunPrivateState->bAdapterPrepareEntered);
        }
        else if (Case == TEXT("EnteredPrepareRetry"))
        {
            if (R->GetRunSnapshot().State == EDivineBeastsDemoRunState::Preparing) return false;
            TestEqual(TEXT("真实Stop故障保留原码"), R->GetRunSnapshot().CleanupResult.Code, FName(TEXT("Test.PreparingStop.Cleanup")));
            TestTrue(TEXT("故障保留适配器/阶段/待清理"), R->HasActiveScenario() && R->RunPrivateState->Adapter.IsValid() && R->RunPrivateState->bAdapterPrepareEntered && R->GetRunSnapshot().bCleanupPending);
            TestTrue(TEXT("失败不释放执行器或外部租约"), F->Data->GetLeaseState(F->OldLease) == EGamePlatformDataRequestState::Succeeded && F->Data->GetLeaseState(F->External) == EGamePlatformDataRequestState::Succeeded);
            TestFalse(TEXT("失败不能开始后继"), R->BeginScenario(F->Scenario));
            TestTrue(TEXT("显式清理重试"), R->RequestScenarioStop(FGamePlatformResult::Success()));
            TestEqual(TEXT("重试真实调用同适配器Stop"), F->First->Stops, 2);
            TestFalse(TEXT("重试成功清除待清理和阶段"), R->HasActiveScenario() || R->RunPrivateState->bAdapterPrepareEntered);
            TestTrue(TEXT("重试仅释放本Run资源"), F->Data->GetLeaseState(F->OldLease) == EGamePlatformDataRequestState::Released && F->Data->GetLeaseState(F->External) == EGamePlatformDataRequestState::Succeeded);
            TestTrue(TEXT("原取消意图不被重试Success改写"), R->GetRunSnapshot().TerminalResult.Status == EGamePlatformResultStatus::Cancelled);
        }
        else
        {
            if (Case == TEXT("WorldExitEntered") && !F->bDestroyed)
            {
                if (R->GetRunSnapshot().State == EDivineBeastsDemoRunState::Preparing) return false;
                TestTrue(TEXT("进入Prepare后才退出"), R->RunPrivateState->bAdapterPrepareEntered && F->First->Prepares == 1);
                F->Destroy();
            }
            TestTrue(TEXT("真实World退出关闭中央运行"), F->bDestroyed && R->RunPrivateState->bClosing);
            TestFalse(TEXT("退出清除阶段及执行器租约"), R->RunPrivateState->bAdapterPrepareEntered || R->RunPrivateState->ResourceLease.IsValid());
            TestEqual(TEXT("退出只销毁本适配器一次"), F->First->Destructions, 1);
            TestEqual(TEXT("退出不等待Stop回调"), F->First->Stops, 0);
            TestTrue(TEXT("退出保存取消而非恢复成功"), R->GetRunSnapshot().State == EDivineBeastsDemoRunState::Cancelled);
            const auto Closed = R->GetRunSnapshot();
            R->OnScenarioResourcesReady(F->OldToken, F->OldLease, FGamePlatformResult::Success());
            R->OnScenarioPrepared(F->OldToken, FGamePlatformResult::Success());
            TestTrue(TEXT("退出后旧回调不能恢复运行"), R->GetRunSnapshot().Token == Closed.Token && R->GetRunSnapshot().State == Closed.State && !R->HasActiveScenario());
            TestTrue(TEXT("退出后本租约释放、外部仍成功"), F->Data->GetLeaseState(F->OldLease) == EGamePlatformDataRequestState::Released && F->Data->GetLeaseState(F->External) == EGamePlatformDataRequestState::Succeeded);
        }
        TestTrue(TEXT("每条路径结束外部租约均保持成功"), F->Data->GetLeaseState(F->External) == EGamePlatformDataRequestState::Succeeded);
        return true;
    }));
    return true;
}
