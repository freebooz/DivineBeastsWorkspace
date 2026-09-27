#if WITH_DEV_AUTOMATION_TESTS
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetworkDelegates.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 真实Data服务+真实已保存流程资产；仅节点执行函数是明确的测试等待替身，不模拟租约成功。 */
class FFlowLeaseIntegrationCommand final : public IAutomationLatentCommand
{
public:
    FFlowLeaseIntegrationCommand(FAutomationTestBase* InTest, FPrimaryAssetId InId)
        : Test(InTest), Id(InId), StartedSeconds(FPlatformTime::Seconds()) {}
    virtual ~FFlowLeaseIntegrationCommand() override { Alive.Reset(); Cleanup(); }
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test->AddError(TEXT("Flow真实租约测试超过30秒，不能视为成功。")); Cleanup(); return true; }
        if (Phase == 0)
        {
            if (!GEngine) { Test->AddError(TEXT("测试需要已初始化的真实UE引擎。")); return true; }
            // 正常初始化引擎内实例以取得它自己的子系统集合；不是新建工程宿主或私有门面替身。
            SavedTokenDelegate = FNetDelegates::OnReceivedNetworkEncryptionToken;
            SavedAckDelegate = FNetDelegates::OnReceivedNetworkEncryptionAck;
            SavedFailureDelegate = FNetDelegates::OnReceivedNetworkEncryptionFailure;
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->Init(); bInitialized = true;
            Service = Instance->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
            Data = IGamePlatformDataService::Get(*Instance.Get());
            if (!Service || !Data) { Test->AddError(TEXT("真实实例未创建Flow或Data服务。")); Cleanup(); return true; }
            // 旧图在资产运行前装配，随后验证资产模式没有覆写legacy配置或节点。
            auto* LegacyNode = NewObject<UGamePlatformCallbackFlowNode>(Instance.Get());
            LegacyNode->Bind([this](const auto& Context, auto Done)
            { ++LegacyStarts; Test->TestFalse(TEXT("legacy输入为空"), Context.InputDefinitionId.IsValid()); Done(FGamePlatformFlowNodeResult::Success()); }, [](auto) {});
            FGamePlatformFlowDefinition Legacy; Legacy.EntryNodeId = TEXT("Legacy");
            FGamePlatformFlowStep Step; Step.NodeId = TEXT("Legacy"); Step.Node = LegacyNode; Legacy.Steps.Add(Step);
            FString Error;
            if (!Service->Configure(Legacy, Error)) { Test->AddError(Error); Cleanup(); return true; }
            Acquire(); Phase = 1; return false;
        }
        if (Phase == 1)
        {
            if (!bAcquired) return false;
            if (!AcquiredResult.IsSuccess()) { Test->AddError(AcquiredResult.Message); Cleanup(); return true; }
            const auto* Definition = Cast<UGamePlatformFlowDefinition>(Data->GetLoadedDefinition(ReadyLease));
            if (!Definition) { Test->AddError(TEXT("参数未指向真实流程资产。")); Cleanup(); return true; }
            for (const auto& Node : Definition->Nodes)
                if (Node.NodeId == Definition->EntryNodeId) ExpectedInput = Node.InputDefinitionId;
            const FGuid OriginalLeaseId = ReadyLease.LeaseId;
            FGamePlatformResult Result;
            Test->TestFalse(TEXT("未注册执行器必须拒绝"), Service->StartFlow(ReadyLease, nullptr, Result).IsValid());
            Test->TestEqual(TEXT("预检失败不转移原租约"), ReadyLease.LeaseId, OriginalLeaseId);
            Test->TestEqual(TEXT("全部工厂预检前不得创建任何节点"), CreatedNodes.Num(), 0);
            TSet<FName> Executors;
            for (const auto& Node : Definition->Nodes) Executors.Add(Node.ExecutorId);
            for (FName ExecutorId : Executors)
            {
                const auto Handle = Service->RegisterNodeFactory(ExecutorId, [this, WeakAlive = TWeakPtr<int32>(Alive)](UGameInstance& Owner)
                {
                    if (!WeakAlive.IsValid()) return static_cast<UGamePlatformFlowNode*>(nullptr);
                    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(&Owner);
                    Test->TestFalse(TEXT("每run创建新节点身份"), CreatedNodes.Contains(TWeakObjectPtr<UGamePlatformFlowNode>(Node)));
                    CreatedNodes.Add(Node);
                    Node->Bind([this, WeakAlive](const auto& Context, auto Done)
                    { if (WeakAlive.IsValid()) { ++Starts; CurrentContext = Context; LateCompletion = MoveTemp(Done); } },
                        [this, WeakAlive](auto) { if (WeakAlive.IsValid()) ++Finishes; });
                    return static_cast<UGamePlatformFlowNode*>(Node);
                }, Result);
                if (!Handle.IsValid()) { Test->AddError(Result.Message); Cleanup(); return true; }
            }
            if (!StartAsset()) { Cleanup(); return true; }
            Phase = 2; return false;
        }
        if (Phase == 2)
        {
            if (Starts < 1) return false;
            Test->TestEqual(TEXT("资产输入传入节点上下文"), CurrentContext.InputDefinitionId, ExpectedInput);
            FGamePlatformResult Result;
            const FGamePlatformFlowNodeToken Token{CurrentContext.Handle, CurrentContext.NodeId, CurrentContext.NodeGeneration};
            Test->TestTrue(TEXT("按完整令牌取消真实资产流程"), Service->CancelFlow(Token, Result));
            Test->TestNull(TEXT("取消终态释放定义租约"), Data->GetLoadedDefinition(ObservedLease));
            Test->TestEqual(TEXT("已开始节点只清理一次"), Finishes, 1);
            FString Error;
            if (!Service->Start(nullptr, Error).IsValid()) { Test->AddError(Error); Cleanup(); return true; }
            Phase = 3; return false;
        }
        if (Phase == 3)
        {
            if (Service->GetSnapshot().State == EGamePlatformFlowState::Running) return false;
            Test->TestTrue(TEXT("资产终结后旧DAG仍成功"), Service->GetSnapshot().State == EGamePlatformFlowState::Succeeded && LegacyStarts == 1);
            Acquire(); Phase = 4; return false;
        }
        if (Phase == 4 || Phase == 6)
        {
            if (!bAcquired) return false;
            if (!AcquiredResult.IsSuccess()) { Test->AddError(AcquiredResult.Message); Cleanup(); return true; }
            if (!StartAsset()) { Cleanup(); return true; }
            ++Phase; return false;
        }
        if (Phase == 5)
        {
            if (Starts < 2) return false;
            FGamePlatformResult Result;
            const FGamePlatformFlowNodeToken Token{CurrentContext.Handle, CurrentContext.NodeId, CurrentContext.NodeGeneration};
            Test->TestTrue(TEXT("失败事件进入真实邮箱"), Service->SubmitEvent(Token,
                FGamePlatformFlowNodeResult::Failure(TEXT("TestNodeFailed"), TEXT("测试限定节点失败。")), Result));
            Phase = 8; return false;
        }
        if (Phase == 8)
        {
            if (Service->GetSnapshot().State == EGamePlatformFlowState::Running) return false;
            Test->TestTrue(TEXT("真实失败保持失败终态"), Service->GetSnapshot().State == EGamePlatformFlowState::Failed);
            Test->TestNull(TEXT("失败终态释放定义租约"), Data->GetLoadedDefinition(ObservedLease));
            Acquire(); Phase = 6; return false;
        }
        if (Starts < 3) return false;
        Service->Deinitialize();
        if (LateCompletion) LateCompletion(FGamePlatformFlowNodeResult::Success());
        Test->TestNull(TEXT("作用域关闭释放租约"), Data->GetLoadedDefinition(ObservedLease));
        Test->TestEqual(TEXT("三个run各清理一次"), Finishes, 3);
        Test->TestEqual(TEXT("本实例没有残留流程租约"), Data->GetDiagnostics().ActiveLeases, 0);
        Cleanup(); return true;
    }
private:
    void Acquire()
    {
        bAcquired = false;
        FGamePlatformResult Accepted;
        ReadyLease = Data->AcquireDefinition(Id, UGamePlatformFlowDefinition::StaticClass(), {}, EGamePlatformDataLifetime::Instance,
            Instance.Get(), [this, WeakAlive = TWeakPtr<int32>(Alive)](const auto& Lease, const auto& Result)
            { if (WeakAlive.IsValid()) { ReadyLease = Lease; AcquiredResult = Result; bAcquired = true; } }, Accepted);
        if (!Accepted.IsSuccess()) { AcquiredResult = Accepted; bAcquired = true; }
    }
    bool StartAsset()
    {
        FGamePlatformResult Result;
        ObservedLease = ReadyLease; // 只作诊断，成功转移后不再通过该副本释放。
        const auto Handle = Service->StartFlow(ReadyLease, nullptr, Result);
        if (!Handle.IsValid() || !Result.IsSuccess()) { Test->AddError(Result.Message); return false; }
        Test->TestFalse(TEXT("成功转移后输入句柄清空"), ReadyLease.IsValid());
        Test->TestNotNull(TEXT("运行期间租约仍保活"), Data->GetLoadedDefinition(ObservedLease));
        return true;
    }
    void Cleanup()
    {
        if (!bInitialized) return;
        bInitialized = false;
        if (Service) Service->Deinitialize();
        if (Data && ReadyLease.IsValid()) Data->ReleaseDefinition(ReadyLease);
        Instance->Shutdown();
        // 测试实例的标准Init/Shutdown会替换引擎单播委托；恢复进入测试前的绑定。
        FNetDelegates::OnReceivedNetworkEncryptionToken = SavedTokenDelegate;
        FNetDelegates::OnReceivedNetworkEncryptionAck = SavedAckDelegate;
        FNetDelegates::OnReceivedNetworkEncryptionFailure = SavedFailureDelegate;
        Data = nullptr; Service = nullptr;
    }
    FAutomationTestBase* Test;
    FPrimaryAssetId Id, ExpectedInput;
    double StartedSeconds;
    int32 Phase = 0, Starts = 0, Finishes = 0, LegacyStarts = 0;
    bool bInitialized = false, bAcquired = false;
    TSharedPtr<int32> Alive = MakeShared<int32>(0);
    TStrongObjectPtr<UGameInstance> Instance;
    UGamePlatformApplicationFlowSubsystem* Service = nullptr;
    IGamePlatformDataService* Data = nullptr;
    FGamePlatformDataLease ReadyLease, ObservedLease;
    FGamePlatformResult AcquiredResult;
    FGamePlatformFlowContext CurrentContext;
    FGamePlatformFlowCompletion LateCompletion;
    TSet<TWeakObjectPtr<UGamePlatformFlowNode>> CreatedNodes;
    decltype(FNetDelegates::OnReceivedNetworkEncryptionToken) SavedTokenDelegate;
    decltype(FNetDelegates::OnReceivedNetworkEncryptionAck) SavedAckDelegate;
    decltype(FNetDelegates::OnReceivedNetworkEncryptionFailure) SavedFailureDelegate;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowLeaseIntegrationTest, "GamePlatform.ApplicationFlow.Asset.RealLeaseLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformFlowLeaseIntegrationTest::RunTest(const FString& Parameters)
{
    FString DefinitionText;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GamePlatformFlowTestDefinition="), DefinitionText))
    {
        AddError(TEXT("必须指定-GamePlatformFlowTestDefinition=GamePlatformDefinition:foundation.flow@1并由引擎实际生成资产；缺失不能跳过并记为通过。"));
        return false;
    }
    const FPrimaryAssetId Id(DefinitionText);
    if (!Id.IsValid()) { AddError(TEXT("测试流程主资产身份非法。")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FFlowLeaseIntegrationCommand(this, Id));
    return true;
}
#endif
