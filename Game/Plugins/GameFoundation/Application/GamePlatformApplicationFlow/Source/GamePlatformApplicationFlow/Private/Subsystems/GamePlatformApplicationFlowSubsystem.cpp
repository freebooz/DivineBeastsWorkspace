#include "API/GamePlatformApplicationFlowSubsystem.h"

#include "Engine/GameInstance.h"
#include "Definitions/GamePlatformFlowDefinitionConversion.h"
#include "Execution/ApplicationFlowExecutor.h"
#include "GamePlatformCore.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/GamePlatformFlowNode.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UObjectGlobals.h"

namespace FlowCore = GamePlatform::ApplicationFlow;

namespace
{
std::string ToCoreName(FName Name)
{
    // FName 的相等规则不区分大小写；核心必须规范化，避免图验证与 UE 查询语义不同。
    return Name.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Name.ToString().ToLower()));
}

EGamePlatformFlowState ToPublicState(FlowCore::EFlowState State)
{
    switch (State)
    {
    case FlowCore::EFlowState::Running: return EGamePlatformFlowState::Running;
    case FlowCore::EFlowState::RetryWaiting: return EGamePlatformFlowState::RetryWaiting;
    case FlowCore::EFlowState::Succeeded: return EGamePlatformFlowState::Succeeded;
    case FlowCore::EFlowState::Failed: return EGamePlatformFlowState::Failed;
    case FlowCore::EFlowState::Cancelled: return EGamePlatformFlowState::Cancelled;
    case FlowCore::EFlowState::Shutdown: return EGamePlatformFlowState::Shutdown;
    default: return EGamePlatformFlowState::Idle;
    }
}

EGamePlatformFlowFinishReason ToPublicReason(FlowCore::EFinishReason Reason)
{
    switch (Reason)
    {
    case FlowCore::EFinishReason::Succeeded: return EGamePlatformFlowFinishReason::Succeeded;
    case FlowCore::EFinishReason::Cancelled: return EGamePlatformFlowFinishReason::Cancelled;
    case FlowCore::EFinishReason::TimedOut: return EGamePlatformFlowFinishReason::TimedOut;
    case FlowCore::EFinishReason::Shutdown: return EGamePlatformFlowFinishReason::Shutdown;
    default: return EGamePlatformFlowFinishReason::Failed;
    }
}
}

/** UObject 与纯调度核心的私有适配器；只有游戏线程操作弱对象。 */
class FGamePlatformFlowNodeAdapter final : public FlowCore::IFlowNode
{
public:
    FGamePlatformFlowNodeAdapter(UGamePlatformApplicationFlowSubsystem* InOwner, UGamePlatformFlowNode* InNode,
        FPrimaryAssetId InInputDefinitionId = {})
        : Owner(InOwner), Node(InNode), InputDefinitionId(InInputDefinitionId) {}

    virtual void Start(const FlowCore::FExecutionContext& Context, FlowCore::FCompletion Complete) override
    {
        auto* Service = Owner.Get();
        auto* NodeObject = Node.Get();
        if (!Service || !NodeObject)
        {
            Complete({false, false, {}, "NodeUnavailable", "流程节点或作用域已经失效"});
            return;
        }
        FGamePlatformFlowContext PublicContext;
        PublicContext.Handle = {Service->ScopeId, Context.RunId};
        PublicContext.NodeId = FName(UTF8_TO_TCHAR(Context.NodeId.c_str()));
        PublicContext.Attempt = Context.Attempt;
        PublicContext.GameInstance = Service->GetGameInstance();
        PublicContext.Payload = Service->ActivePayload;
        PublicContext.NodeGeneration = Context.NodeGeneration;
        PublicContext.InputDefinitionId = InputDefinitionId;
        NodeObject->Execute(PublicContext, [Complete = std::move(Complete)](FGamePlatformFlowNodeResult Result)
        {
            // 完成仅转换值并投递邮箱，绝不从工作线程解引用 Owner 或 Node。
            Complete({Result.bSucceeded, Result.bRetryable, ToCoreName(Result.Outcome),
                Result.ErrorCode.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Result.ErrorCode.ToString())),
                std::string(TCHAR_TO_UTF8(*Result.ErrorMessage))});
        });
    }

    virtual void Finish(FlowCore::EFinishReason Reason) override
    {
        if (auto* NodeObject = Node.Get()) NodeObject->Finish(ToPublicReason(Reason));
    }

private:
    TWeakObjectPtr<UGamePlatformApplicationFlowSubsystem> Owner;
    TWeakObjectPtr<UGamePlatformFlowNode> Node;
    FPrimaryAssetId InputDefinitionId;
};

UGamePlatformApplicationFlowSubsystem::UGamePlatformApplicationFlowSubsystem() = default;
UGamePlatformApplicationFlowSubsystem::UGamePlatformApplicationFlowSubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformApplicationFlowSubsystem::~UGamePlatformApplicationFlowSubsystem() = default;

bool UGamePlatformApplicationFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return !IsRunningCommandlet() && Cast<UGameInstance>(Outer) != nullptr && Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformApplicationFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ScopeId = FGuid::NewGuid();
    bClosing = false;
    bDeinitialized = false;
    LastPublishedRunId = 0;
    Executor = MakeUnique<FlowCore::FApplicationFlowExecutor>();
}

void UGamePlatformApplicationFlowSubsystem::RemoveTicker()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
}

void UGamePlatformApplicationFlowSubsystem::Deinitialize()
{
    check(IsInGameThread());
    if (bClosing) return; // Finish(Shutdown) 或广播中再次退出不得递归销毁。
    bClosing = true;
    RemoveTicker();
    CompleteDeinitialize();
}

void UGamePlatformApplicationFlowSubsystem::CompleteDeinitialize()
{
    if (!bClosing || bDeinitialized || bDispatching || bPublishing) return;
    bDeinitialized = true;
    if (Executor)
    {
        const bool bWasActive = Executor->IsActive();
        // 若 Finish 已经完成成功／取消，先保留该终态；Shutdown 不覆盖尚未发布的业务结果。
        if (!bWasActive) PublishTerminal();
        {
            TGuardValue<bool> DispatchGuard(bDispatching, true);
            if (!ensureMsgf(Executor->Shutdown(), TEXT("退出必须在执行器分发栈展开后执行")))
            {
                bDeinitialized = false;
                return; // 未完成清理时保留对象，不把失败关闭变成释放后访问。
            }
        }
        if (bWasActive) PublishTerminal();
        Executor.Reset();
    }
    ActivePayload = nullptr;
    ReleaseAssetRun();
    OwnedNodes.Reset();
    LegacyNodes.Reset();
    PendingAssetNodes.Reset();
    LegacyCoreDefinition.Reset();
    NodeFactories.Reset();
    PreviouslyCreatedNodes.Reset();
    FinishedEvent.Clear();
    Super::Deinitialize();
}

bool UGamePlatformApplicationFlowSubsystem::CanControl(FString& OutError) const
{
    OutError.Reset();
    if (!IsInGameThread()) OutError = TEXT("流程控制必须在游戏线程执行");
    else if (bClosing || !Executor) OutError = TEXT("流程作用域未初始化或正在关闭");
    else if (bPublishing || bDispatching) OutError = TEXT("节点或终态事件中禁止重入；请投递下一游戏线程任务");
    return OutError.IsEmpty();
}

bool UGamePlatformApplicationFlowSubsystem::Configure(const FGamePlatformFlowDefinition& Definition, FString& OutError)
{
    if (!CanControl(OutError)) return false;
    FlowCore::FDefinition CoreDefinition;
    CoreDefinition.Entry = ToCoreName(Definition.EntryNodeId);
    TArray<TObjectPtr<UGamePlatformFlowNode>> NewNodes;
    for (const auto& Step : Definition.Steps)
    {
        if (!IsValid(Step.Node) || Step.Node->GetTypedOuter<UGameInstance>() != GetGameInstance() || NewNodes.Contains(Step.Node))
        {
            OutError = TEXT("节点不存在、重复使用或不属于本 GameInstance");
            return false;
        }
        NewNodes.Add(Step.Node);
        FlowCore::FStep CoreStep;
        CoreStep.Id = ToCoreName(Step.NodeId);
        CoreStep.Node = std::make_shared<FGamePlatformFlowNodeAdapter>(this, Step.Node);
        CoreStep.Next = ToCoreName(Step.NextNodeId);
        CoreStep.TimeoutSeconds = Step.TimeoutSeconds;
        CoreStep.MaxAttempts = Step.MaxAttempts;
        CoreStep.RetryDelaySeconds = Step.RetryDelaySeconds;
        CoreStep.bRetryOnTimeout = Step.bRetryOnTimeout;
        for (const auto& Pair : Step.Routes) CoreStep.Routes.emplace(ToCoreName(Pair.Key), ToCoreName(Pair.Value));
        CoreDefinition.Steps.push_back(std::move(CoreStep));
    }
    std::string Error;
    const auto SavedLegacyDefinition = CoreDefinition;
    if (!Executor->Configure(std::move(CoreDefinition), Error))
    {
        OutError = UTF8_TO_TCHAR(Error.c_str());
        return false;
    }
    OwnedNodes = MoveTemp(NewNodes);
    LegacyNodes = OwnedNodes;
    LegacyCoreDefinition = MakeUnique<FlowCore::FDefinition>(SavedLegacyDefinition);
    bAssetConfiguration = false;
    return true;
}

FGamePlatformFlowHandle UGamePlatformApplicationFlowSubsystem::Start(UObject* Payload, FString& OutError)
{
    if (!CanControl(OutError)) return {};
    if (Payload && (!IsValid(Payload) || Payload->GetTypedOuter<UGameInstance>() != GetGameInstance()))
    {
        OutError = TEXT("上下文对象必须属于本 GameInstance，且不能使用世界 Actor 作为跨地图载荷");
        return {};
    }
    std::string Error;
    if (bAssetConfiguration)
    {
        if (!LegacyCoreDefinition)
        {
            OutError = TEXT("资产运行必须使用StartFlow和新的就绪租约；旧Start须先Configure");
            return {};
        }
        if (!Executor->Configure(*LegacyCoreDefinition, Error)) { OutError = UTF8_TO_TCHAR(Error.c_str()); return {}; }
        OwnedNodes = LegacyNodes;
        bAssetConfiguration = false;
    }
    const auto RunId = Executor->Start(FPlatformTime::Seconds(), Error);
    if (!RunId) { OutError = UTF8_TO_TCHAR(Error.c_str()); return {}; }
    ActivePayload = Payload;
    // 只在活动流程期间注册。Ticker 用于回到游戏线程、超时和退避，不做业务逐帧计算。
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,
        &UGamePlatformApplicationFlowSubsystem::TickFlow));
    return {ScopeId, RunId};
}

bool UGamePlatformApplicationFlowSubsystem::Cancel(const FGamePlatformFlowHandle& Handle)
{
    FString Error;
    if (!CanControl(Error) || Handle.ScopeId != ScopeId || !Handle.IsValid()) return false;
    bool bCancelled = false;
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        bCancelled = Executor->Cancel(Handle.RunId);
    }
    if (bClosing) { CompleteDeinitialize(); return bCancelled; }
    if (bCancelled) { RemoveTicker(); PublishTerminal(); }
    return bCancelled;
}

FGamePlatformFlowSnapshot UGamePlatformApplicationFlowSubsystem::GetSnapshot() const
{
    check(IsInGameThread());
    FGamePlatformFlowSnapshot Result;
    Result.Handle.ScopeId = ScopeId;
    if (!Executor)
    {
        Result.State = bClosing ? EGamePlatformFlowState::Shutdown : EGamePlatformFlowState::Idle;
        return Result;
    }
    const auto& CoreSnapshot = Executor->GetSnapshot();
    Result.State = ToPublicState(CoreSnapshot.State);
    Result.Handle.RunId = CoreSnapshot.RunId;
    Result.NodeId = FName(UTF8_TO_TCHAR(CoreSnapshot.NodeId.c_str()));
    Result.Attempt = CoreSnapshot.Attempt;
    Result.NodeGeneration = CoreSnapshot.NodeGeneration;
    Result.ErrorCode = FName(UTF8_TO_TCHAR(CoreSnapshot.ErrorCode.c_str()));
    Result.ErrorMessage = UTF8_TO_TCHAR(CoreSnapshot.ErrorMessage.c_str());
    return Result;
}

bool UGamePlatformApplicationFlowSubsystem::TickFlow(float DeltaSeconds)
{
    if (bClosing || !Executor) return false;
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        auto* DataService = ActiveDefinitionLease.IsValid() ? IGamePlatformDataService::Get(*GetGameInstance()) : nullptr;
        if (ActiveDefinitionLease.IsValid() && (!DataService || !DataService->GetLoadedDefinition(ActiveDefinitionLease)))
            Executor->FailRun(Executor->GetSnapshot().RunId, "FlowLeaseExpired", "活动流程的定义租约已失效");
        else
            Executor->Tick(FPlatformTime::Seconds());
    }
    if (bClosing) { CompleteDeinitialize(); return false; }
    if (!Executor->IsActive())
    {
        TickerHandle.Reset(); // 返回 false 后由 Ticker 自己移除当前回调。
        PublishTerminal();
        return false;
    }
    return true;
}

void UGamePlatformApplicationFlowSubsystem::PublishTerminal()
{
    if (!Executor || Executor->IsActive()) return;
    const auto Snapshot = GetSnapshot();
    if (Snapshot.Handle.RunId == 0 || Snapshot.Handle.RunId == LastPublishedRunId) return;
    LastPublishedRunId = Snapshot.Handle.RunId;
    ActivePayload = nullptr;
    {
        TGuardValue<bool> PublishGuard(bPublishing, true);
        ReleaseAssetRun();
        FinishedEvent.Broadcast(Snapshot);
    }
    if (bClosing) CompleteDeinitialize();
}

FGamePlatformFlowFactoryHandle UGamePlatformApplicationFlowSubsystem::RegisterNodeFactory(
    FName ExecutorId, FGamePlatformFlowNodeFactory Factory, FGamePlatformResult& OutResult)
{
    FString Error;
    if (!CanControl(Error)) { OutResult = FGamePlatformResult::Failure(TEXT("FlowUnavailable"), Error); return {}; }
    if (Executor->IsActive() || ExecutorId.IsNone() || !Factory || NodeFactories.Contains(ExecutorId))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidFactoryRegistration"), TEXT("运行中、空键、空工厂或重复执行器键不能注册。"));
        return {};
    }
    const FGuid RegistrationId = FGuid::NewGuid();
    NodeFactories.Add(ExecutorId, FFactoryRegistration{RegistrationId, MoveTemp(Factory)});
    OutResult = FGamePlatformResult::Success();
    return {ScopeId, RegistrationId, ExecutorId};
}

bool UGamePlatformApplicationFlowSubsystem::UnregisterNodeFactory(
    const FGamePlatformFlowFactoryHandle& Handle, FGamePlatformResult& OutResult)
{
    FString Error;
    if (!CanControl(Error)) { OutResult = FGamePlatformResult::Failure(TEXT("FlowUnavailable"), Error); return false; }
    const auto* Registration = NodeFactories.Find(Handle.ExecutorId);
    if (Executor->IsActive() || !Handle.IsValid() || Handle.ScopeId != ScopeId || !Registration || Registration->RegistrationId != Handle.RegistrationId)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidFactoryHandle"), TEXT("工厂撤销必须空闲且匹配当前作用域和注册代次。"));
        return false;
    }
    NodeFactories.Remove(Handle.ExecutorId);
    OutResult = FGamePlatformResult::Success();
    return true;
}

FGamePlatformFlowHandle UGamePlatformApplicationFlowSubsystem::StartFlow(
    FGamePlatformDataLease& InOutReadyLease, UObject* Payload, FGamePlatformResult& OutResult)
{
    FString Error;
    if (!CanControl(Error)) { OutResult = FGamePlatformResult::Failure(TEXT("FlowUnavailable"), Error); return {}; }
    if (Executor->IsActive()) { OutResult = FGamePlatformResult::Failure(TEXT("FlowBusy"), TEXT("已有活动主流程。")); return {}; }
    if (Payload && (!IsValid(Payload) || Payload->GetTypedOuter<UGameInstance>() != GetGameInstance()))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidFlowPayload"), TEXT("载荷必须属于本GameInstance。"));
        return {};
    }
    auto* DataService = IGamePlatformDataService::Get(*GetGameInstance());
    if (!DataService || !InOutReadyLease.IsValid() || DataService->GetLeaseState(InOutReadyLease) != EGamePlatformDataRequestState::Succeeded)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("FlowLeaseNotReady"), TEXT("流程需要本实例数据服务成功且尚未释放的定义租约。"));
        return {};
    }
    const auto* Definition = Cast<UGamePlatformFlowDefinition>(DataService->GetLoadedDefinition(InOutReadyLease));
    if (!Definition) { OutResult = FGamePlatformResult::Failure(TEXT("InvalidFlowDefinitionType"), TEXT("租约未持有流程定义资产。")); return {}; }
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        OutResult = Definition->ValidateDefinition();
    }
    if (bClosing) { CompleteDeinitialize(); OutResult = FGamePlatformResult::Failure(TEXT("FlowClosing"), TEXT("定义校验期间作用域关闭。")); return {}; }
    if (!OutResult.IsSuccess()) return {};
    if (DataService->GetLoadedDefinition(InOutReadyLease) != Definition)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("FlowLeaseExpired"), TEXT("定义校验期间租约失效。")); return {};
    }
    auto CoreDefinition = FlowCore::ConvertAssetGraph(*Definition);
    std::string GraphError;
    if (Definition->MaxImmediateCycleTransitions < 1 || !FlowCore::FApplicationFlowExecutor::ValidateGraph(CoreDefinition, GraphError))
    {
        // 即使派生资产遗漏Super校验，任何工厂执行前仍须由真正的执行器独立检查完整图。
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidFlowGraph"), GraphError.empty()
            ? FString(TEXT("即时循环预算必须为正数。")) : FString(UTF8_TO_TCHAR(GraphError.c_str())));
        return {};
    }
    const TArray<FGamePlatformFlowNodeDefinition> AssetNodes = Definition->Nodes;
    TArray<FGamePlatformFlowNodeFactory> Factories;
    // 全图与全部执行器均验证后才调用任何工厂；不允许先启动部分节点再发现缺失能力。
    for (const auto& AssetNode : AssetNodes)
    {
        const auto* Registration = NodeFactories.Find(AssetNode.ExecutorId);
        if (!Registration)
        {
            OutResult = FGamePlatformResult::Failure(TEXT("MissingNodeFactory"), FString::Printf(TEXT("未注册节点执行器：%s"), *AssetNode.ExecutorId.ToString()));
            return {};
        }
        Factories.Add(Registration->Factory);
    }
    for (auto It = PreviouslyCreatedNodes.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    PendingAssetNodes.Reset();
    for (int32 Index = 0; Index < AssetNodes.Num(); ++Index)
    {
        UGamePlatformFlowNode* Node = nullptr;
        {
            TGuardValue<bool> DispatchGuard(bDispatching, true);
            Node = Factories[Index](*GetGameInstance());
        }
        if (bClosing)
        {
            CompleteDeinitialize();
            OutResult = FGamePlatformResult::Failure(TEXT("FlowClosing"), TEXT("节点工厂执行期间作用域关闭。")); return {};
        }
        if (!IsValid(Node) || Node->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
            Node->GetTypedOuter<UGameInstance>() != GetGameInstance() || LegacyNodes.Contains(Node) ||
            PreviouslyCreatedNodes.Contains(TWeakObjectPtr<UGamePlatformFlowNode>(Node)))
        {
            PendingAssetNodes.Reset();
            OutResult = FGamePlatformResult::Failure(TEXT("InvalidFactoryNode"), TEXT("工厂必须为每次运行返回本GameInstance新建的独立节点，不能复用或返回类默认对象。")); return {};
        }
        PendingAssetNodes.Add(Node);
        PreviouslyCreatedNodes.Add(Node);
        CoreDefinition.Steps[Index].Node = std::make_shared<FGamePlatformFlowNodeAdapter>(this, Node, AssetNodes[Index].InputDefinitionId);
    }
    if (DataService->GetLoadedDefinition(InOutReadyLease) != Definition)
    {
        PendingAssetNodes.Reset();
        OutResult = FGamePlatformResult::Failure(TEXT("FlowLeaseExpired"), TEXT("创建节点期间租约失效，启动未接纳。")); return {};
    }
    std::string CoreError;
    const uint64 RunId = Executor->ConfigureAndStart(std::move(CoreDefinition), FPlatformTime::Seconds(), CoreError);
    if (RunId == 0)
    {
        PendingAssetNodes.Reset();
        OutResult = FGamePlatformResult::Failure(TEXT("FlowStartRejected"), UTF8_TO_TCHAR(CoreError.c_str())); return {};
    }
    // 此后才转移所有权；Start只登记，节点Execute要等下一次调度，不存在半接纳执行。
    OwnedNodes = MoveTemp(PendingAssetNodes);
    ActiveDefinitionLease = MoveTemp(InOutReadyLease);
    InOutReadyLease = {};
    ActivePayload = Payload;
    bAssetConfiguration = true;
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,
        &UGamePlatformApplicationFlowSubsystem::TickFlow));
    OutResult = FGamePlatformResult::Success();
    return {ScopeId, RunId};
}

bool UGamePlatformApplicationFlowSubsystem::CancelFlow(const FGamePlatformFlowNodeToken& Token, FGamePlatformResult& OutResult)
{
    FString Error;
    if (!CanControl(Error)) { OutResult = FGamePlatformResult::Failure(TEXT("FlowUnavailable"), Error); return false; }
    if (!Token.IsValid() || Token.Handle.ScopeId != ScopeId)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidNodeToken"), TEXT("节点令牌无效或属于其他实例。")); return false;
    }
    bool bCancelled;
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        bCancelled = Executor->CancelNode(Token.Handle.RunId, ToCoreName(Token.NodeId), Token.NodeGeneration);
    }
    OutResult = bCancelled ? FGamePlatformResult::Success() :
        FGamePlatformResult::Failure(TEXT("StaleNodeToken"), TEXT("当前节点未开始、已结束或令牌代次过期。"));
    if (bClosing) CompleteDeinitialize();
    else if (bCancelled) { RemoveTicker(); PublishTerminal(); }
    return bCancelled;
}

bool UGamePlatformApplicationFlowSubsystem::SubmitEvent(const FGamePlatformFlowNodeToken& Token,
    FGamePlatformFlowNodeResult Event, FGamePlatformResult& OutResult)
{
    FString Error;
    if (!CanControl(Error)) { OutResult = FGamePlatformResult::Failure(TEXT("FlowUnavailable"), Error); return false; }
    if (!Token.IsValid() || Token.Handle.ScopeId != ScopeId)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidNodeToken"), TEXT("节点事件令牌无效或属于其他实例。")); return false;
    }
    const bool bAccepted = Executor->SubmitEvent(Token.Handle.RunId, ToCoreName(Token.NodeId), Token.NodeGeneration,
        {Event.bSucceeded, Event.bRetryable, ToCoreName(Event.Outcome),
            Event.ErrorCode.IsNone() ? std::string{} : std::string(TCHAR_TO_UTF8(*Event.ErrorCode.ToString())),
            std::string(TCHAR_TO_UTF8(*Event.ErrorMessage))});
    OutResult = bAccepted ? FGamePlatformResult::Success() :
        FGamePlatformResult::Failure(TEXT("StaleOrCompletedNode"), TEXT("节点代次过期、尚未开始，或该节点已接纳首次完成。"));
    return bAccepted;
}

void UGamePlatformApplicationFlowSubsystem::ReleaseAssetRun()
{
    if (!ActiveDefinitionLease.IsValid()) return;
    const FGamePlatformDataLease Lease = MoveTemp(ActiveDefinitionLease);
    ActiveDefinitionLease = {};
    if (auto* DataService = IGamePlatformDataService::Get(*GetGameInstance()))
    {
        const auto ReleaseResult = DataService->ReleaseDefinition(Lease);
        if (!ReleaseResult.IsSuccess())
            UE_LOG(LogGamePlatformCore, Error, TEXT("流程定义租约释放失败：%s %s"), *ReleaseResult.Code.ToString(), *ReleaseResult.Message);
    }
    else
    {
        // Data先关闭时由Data自己的作用域清理撤销全部租约，不尝试访问销毁后的服务。
        UE_LOG(LogGamePlatformCore, Warning, TEXT("流程退出时数据服务不可用；租约由数据作用域关闭路径清理。"));
    }
    OwnedNodes.Reset();
}
