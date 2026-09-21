#include "API/GamePlatformApplicationFlowSubsystem.h"

#include "Engine/GameInstance.h"
#include "Execution/ApplicationFlowExecutor.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/GamePlatformFlowNode.h"
#include "Misc/GuardValue.h"
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
    FGamePlatformFlowNodeAdapter(UGamePlatformApplicationFlowSubsystem* InOwner, UGamePlatformFlowNode* InNode)
        : Owner(InOwner), Node(InNode) {}

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
};

UGamePlatformApplicationFlowSubsystem::UGamePlatformApplicationFlowSubsystem() = default;
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
    bClosing = true;
    RemoveTicker();
    if (Executor)
    {
        const bool bWasActive = Executor->IsActive();
        {
            TGuardValue<bool> DispatchGuard(bDispatching, true);
            Executor->Shutdown();
        }
        if (bWasActive) PublishTerminal();
        Executor.Reset();
    }
    ActivePayload = nullptr;
    OwnedNodes.Reset();
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
    if (!Executor->Configure(std::move(CoreDefinition), Error))
    {
        OutError = UTF8_TO_TCHAR(Error.c_str());
        return false;
    }
    OwnedNodes = MoveTemp(NewNodes);
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
    Result.ErrorCode = FName(UTF8_TO_TCHAR(CoreSnapshot.ErrorCode.c_str()));
    Result.ErrorMessage = UTF8_TO_TCHAR(CoreSnapshot.ErrorMessage.c_str());
    return Result;
}

bool UGamePlatformApplicationFlowSubsystem::TickFlow(float DeltaSeconds)
{
    if (bClosing || !Executor) return false;
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        Executor->Tick(FPlatformTime::Seconds());
    }
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
    TGuardValue<bool> PublishGuard(bPublishing, true);
    FinishedEvent.Broadcast(Snapshot);
}
