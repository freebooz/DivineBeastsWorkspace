// 隔离GameInstance的真实平台Flow人工操作，避免覆盖项目唯一正式应用主流程。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Scenarios/DivineBeastsDemoManualFlowNode.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/NetworkDelegates.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#endif
namespace DivineBeasts::Demo
{
struct FManualFlowLeaseState
{
    FDivineBeastsDemoRunToken Token;TWeakObjectPtr<UGameInstance> GI;TWeakObjectPtr<UWorld> World;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    FGamePlatformDataLease Lease;uint64 Generation=0;bool bAlive=true,bLoading=false,bReady=false;
};
class FManualFlowScenario final : public FPlatformManualBase
{
public:
    FManualFlowScenario(UWorld& W,FName F):FPlatformManualBase(W,F),DataState(MakeShared<FManualFlowLeaseState>()){}
    ~FManualFlowScenario() override {CleanupService();}
private:
    FGamePlatformResult PrepareService() override
    {
        if(!GEngine)return ManualError(TEXT("Demo.FlowEngineMissing"),TEXT("隔离Flow需要真实Engine宿主。"));
        Host=Instance();
        if(!Host.IsValid())return ManualError(TEXT("Demo.FlowActualGIRequired"),TEXT("需要真实GI。"));
        HostLease=Host->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
        if(!HostLease.IsValid())return ManualError(TEXT("Demo.FlowHostCollection"),TEXT("真实GI缺少受控宿主集合。"));
        const auto Owned=HostLease->Acquire(Context.Token,*SourceWorld);if(!Owned.IsSuccess())return Owned;
        Flow=Host->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
        if(!Flow.IsValid() || !IGamePlatformDataService::Get(*Host))return ManualError(TEXT("Demo.FlowActualCollection"),TEXT("同GI唯一Flow/Data未由真实SubsystemCollection装配。"));
        const auto Existing=Flow->GetSnapshot();
        if(Existing.State==EGamePlatformFlowState::Running || Existing.State==EGamePlatformFlowState::RetryWaiting)return ManualError(TEXT("Demo.FlowAlreadyRunning"),TEXT("不接管已有流程。"));
        DataState->Token=Context.Token;DataState->World=SourceWorld;DataState->GI=Host;DataState->Publish=Context.PublishObservation;
        Node.Reset(NewObject<UDivineBeastsDemoManualFlowNode>(Host.Get()));
        // 事件只记录值；不从广播中同步重入Flow控制。
        Observed=Flow->GetSnapshot();
        Changed=Flow->OnSnapshotChanged().AddLambda([this](const auto& Value){if(Alive()){Observed=Value;++Events;
            if(Context.PublishObservation)Context.PublishObservation(Context.Token,FString::Printf(TEXT("Flow State=%d Node=%s Generation=%llu Error=%s"),
                int32(Value.State),*Value.NodeId.ToString(),Value.NodeGeneration,*Value.ErrorCode.ToString()));}});
        return ManualObserved(TEXT("真实GI唯一Flow已取得显式测试租约；空闲且生产Bootstrap由DemoMode禁用。"));
    }
    FGamePlatformResult CleanupService() override
    {
        if(Flow.IsValid())
        {
            Flow->OnSnapshotChanged().Remove(Changed);
            const auto Current=Flow->GetSnapshot();
            if(Current.State==EGamePlatformFlowState::Running || Current.State==EGamePlatformFlowState::RetryWaiting)
            {
                if(Current.Handle.ScopeId!=Handle.ScopeId || Current.Handle.RunId!=Handle.RunId)return ManualError(TEXT("Demo.FlowOwnerChanged"),TEXT("已有其它流程，不取消他人运行。"));
                if(!Flow->Cancel(Handle))return ManualError(TEXT("Demo.FlowCancelCleanup"),TEXT("本次流程取消失败。"));
            }
            if(Factory.IsValid()){FGamePlatformResult R;if(!Flow->UnregisterNodeFactory(Factory,R))return R;Factory={};}
            Flow.Reset();
        }
        DataState->bAlive=false;++DataState->Generation;
        if(DataState->Lease.IsValid())
        {
            auto* D=DataState->GI.IsValid()?IGamePlatformDataService::Get(*DataState->GI):nullptr;
            if(!D)return ManualError(TEXT("Demo.FlowDataCleanupMissing"),TEXT("保留定义租约，Data失效。"));
            const auto R=D->ReleaseDefinition(DataState->Lease);if(!R.IsSuccess())return R;DataState->Lease={};
        }
        Node.Reset();Host.Reset();
        if(HostLease.IsValid()){const auto R=HostLease->Release(Context.Token);if(!R.IsSuccess())return R;HostLease.Reset();}
        return FGamePlatformResult::Success();
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R;FString Why;
        if(Op==TEXT("AcquireFlowDefinition"))
        {
            if(DataState->Lease.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.FlowDefinitionOwned"),TEXT("先ReleaseFlowDefinition。")));return;}
            FGamePlatformId Id;
            if(A.Int(TEXT("LogicalVersion"))<1 || A.Int(TEXT("LogicalVersion"))>MAX_int32 ||
                !FGamePlatformId::TryCreate(A.Id(TEXT("Namespace")).ToString(),A.Id(TEXT("Name")).ToString(),static_cast<int32>(A.Int(TEXT("LogicalVersion"))),Id))
            {Done(Context.Token,ManualError(TEXT("Demo.FlowDefinitionId"),TEXT("非法流程资产身份。")));return;}
            auto* D=IGamePlatformDataService::Get(*Host);const uint64 G=++DataState->Generation;DataState->bLoading=true;DataState->bReady=false;
            TWeakPtr<FManualFlowLeaseState> Weak(DataState);
            // IGamePlatformDataService正式合同保证下一调度轮或更晚通知，Lease赋值先于回调。
            DataState->Lease=D->AcquireDefinition(FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*Id.ToString())),
                UGamePlatformFlowDefinition::StaticClass(),{},EGamePlatformDataLifetime::Instance,Flow.Get(),
                [Weak,G](const auto& L,const auto& Result)
                {
                    auto V=Weak.Pin();if(!V || !V->bAlive || V->Generation!=G || !V->World.IsValid() || V->World->bIsTearingDown)return;const auto& E=V->Lease;
                    const bool Match=L.ScopeId==E.ScopeId && L.LeaseId==E.LeaseId && L.Generation==E.Generation && L.IssuerProof==E.IssuerProof &&
                        L.DefinitionId==E.DefinitionId && L.Bundles==E.Bundles && L.ResourcePaths==E.ResourcePaths;
                    if(!Match){if(V->Publish)V->Publish(V->Token,TEXT("Demo.FlowDataLeaseIdentityMismatch：忽略其它租约完成。"));return;}
                    V->bLoading=false;V->bReady=Result.IsSuccess();
                    if(V->Publish && V->World.IsValid())V->Publish(V->Token,FString::Printf(TEXT("Flow资产DataReady=%d Status=%d Code=%s"),V->bReady,int32(Result.Status),*Result.Code.ToString()));
                },R);
            if(!R.IsSuccess())DataState->bLoading=false;else R.Message=TEXT("流程定义申请已受理，未就绪；看真实Data完成事件。");
        }
        else if(Op==TEXT("ReleaseFlowDefinition"))
        {
            ++DataState->Generation;DataState->bLoading=false;DataState->bReady=false;
            auto* D=IGamePlatformDataService::Get(*Host);R=D->ReleaseDefinition(DataState->Lease);if(R.IsSuccess())DataState->Lease={};
        }
        else if(Op==TEXT("StartFlow"))
        {
            if(!DataState->bReady){Done(Context.Token,ManualError(TEXT("Demo.FlowDefinitionNotReady"),TEXT("需真实Data租约Succeeded后StartFlow。")));return;}
            const auto Candidate=Flow->StartFlow(DataState->Lease,nullptr,R);
            if(Candidate.IsValid() && R.IsSuccess()){Handle=Candidate;DataState->bReady=false;R.Message=TEXT("真实StartFlow受理，租约已转移至同GI唯一执行器；不代表流程成功。");}
        }
        else if(Op==TEXT("Configure"))
        {
            FGamePlatformFlowDefinition D;D.EntryNodeId=TEXT("ManualWait");
            FGamePlatformFlowStep Step;Step.NodeId=D.EntryNodeId;Step.Node=Node.Get();Step.TimeoutSeconds=A.Number(TEXT("TimeoutSeconds"));
            Step.bWaitForUserInput=A.Bool(TEXT("WaitForUser"));Step.Routes.Add(TEXT("Accept"),NAME_None);D.Steps.Add(Step);
            bLegacyConfigured=Flow->Configure(D,Why);R=bLegacyConfigured?ManualObserved(TEXT("真实Configure接纳；节点尚未执行。")):FGamePlatformResult::Failure(TEXT("Demo.FlowConfigure"),Why);
        }
        else if(Op==TEXT("RegisterFactory"))
        {
            if(Factory.IsValid())R=ManualError(TEXT("Demo.FlowFactoryAlreadyOwned"),TEXT("已持有本运行工厂；重复注册不覆盖原句柄。"));
            else
            {
                const auto Candidate=Flow->RegisterNodeFactory(TEXT("ManualWaitExecutor"),[](UGameInstance& GI){return NewObject<UDivineBeastsDemoManualFlowNode>(&GI);},R);
                if(Candidate.IsValid() && R.IsSuccess())Factory=Candidate;
            }
        }
        else if(Op==TEXT("UnregisterFactory"))
        {if(Flow->UnregisterNodeFactory(Factory,R))Factory={};} // 失败保留；成功不让Stop再次撤销旧句柄。
        else if(Op==TEXT("Start"))
        {
            if(!bLegacyConfigured){Done(Context.Token,ManualError(TEXT("Demo.FlowConfigureFirst"),TEXT("先由本运行Configure，不启动宿主旧配置。")));return;}
            const auto Candidate=Flow->Start(nullptr,Why);
            if(Candidate.IsValid()){Handle=Candidate;R=ManualObserved(TEXT("真实Flow开始；人工等待不代表完成。"));}
            else R=FGamePlatformResult::Failure(TEXT("Demo.FlowStart"),Why);
        }
        else if(Op==TEXT("SubmitBranch") || Op==TEXT("SubmitFailure") || Op==TEXT("CancelNode"))
        {
            FGamePlatformFlowNodeToken Token;Token.Handle=Observed.Handle;Token.NodeId=Observed.NodeId;Token.NodeGeneration=Observed.NodeGeneration;
            if(Op==TEXT("CancelNode"))Flow->CancelFlow(Token,R);
            else Flow->SubmitEvent(Token,Op==TEXT("SubmitBranch")?FGamePlatformFlowNodeResult::Success(A.Id(TEXT("Outcome"))):
                FGamePlatformFlowNodeResult::Failure(TEXT("Demo.ManualNodeFailure"),TEXT("操作者提交的失败事实。")),R);
        }
        else if(Op==TEXT("Cancel"))
            R=Flow->Cancel(Handle)?ManualObserved(TEXT("本句柄取消已接纳。")):ManualError(TEXT("Demo.FlowCancelRejected"),TEXT("真实Flow拒绝旧/空闲句柄取消。"));
        else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
        else R=ManualError(TEXT("Demo.FlowOperation"),TEXT("操作不属于Flow人工白名单。"));
        R.Message+=FString::Printf(TEXT("；State=%d Node=%s Generation=%llu Events=%llu LegacyNodeFinish=%d"),
            int32(Observed.State),*Observed.NodeId.ToString(),Observed.NodeGeneration,Events,Node.IsValid()?Node->GetFinishCount():0);
        Done(Context.Token,R);
    }
#if WITH_DEV_AUTOMATION_TESTS
    friend bool VerifyManualFlowOwnershipForTests(FAutomationTestBase& Test);
    friend class FManualFlowAssetOwnershipCommand;
#endif
    TWeakObjectPtr<UGameInstance> Host;
    TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> HostLease;
    TSharedRef<FManualFlowLeaseState> DataState;
    TWeakObjectPtr<UGamePlatformApplicationFlowSubsystem> Flow;
    TStrongObjectPtr<UDivineBeastsDemoManualFlowNode> Node;
    FDelegateHandle Changed;
    FGamePlatformFlowSnapshot Observed;
    FGamePlatformFlowHandle Handle;
    FGamePlatformFlowFactoryHandle Factory;
    uint64 Events=0;bool bLegacyConfigured=false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformFlowScenario(UWorld& W,FName F)
{return MakeUnique<FManualFlowScenario>(W,F);}
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoManualFlowOwnershipProbe.inl"
#endif

}

