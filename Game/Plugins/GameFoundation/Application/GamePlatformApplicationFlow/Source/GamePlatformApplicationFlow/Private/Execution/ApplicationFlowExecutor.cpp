#include "Execution/ApplicationFlowExecutor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace GamePlatform::ApplicationFlow
{
FApplicationFlowExecutor::FApplicationFlowExecutor()
    : OwnerThread(std::this_thread::get_id()), Mailbox(std::make_shared<FMailbox>())
{
}

FApplicationFlowExecutor::~FApplicationFlowExecutor()
{
    // 宿主须在创建线程销毁；先关闭再释放节点，迟到回调只捕获弱邮箱。
    Shutdown();
}

bool FApplicationFlowExecutor::IsActive() const
{
    return Snapshot.State == EFlowState::Running || Snapshot.State == EFlowState::RetryWaiting;
}

bool FApplicationFlowExecutor::CheckControl(std::string& Error) const
{
    Error.clear();
    if (std::this_thread::get_id() != OwnerThread) Error = "流程控制仅允许所有者线程调用";
    else if (bInNodeCallback) Error = "节点回调内不允许重入流程控制；请投递下一次游戏线程任务";
    else if (Snapshot.State == EFlowState::Shutdown) Error = "流程执行器已经关闭";
    return Error.empty();
}

bool FApplicationFlowExecutor::Validate(const FDefinition& InDefinition, std::string& Error)
{
    std::map<std::string, const FStep*> Steps;
    std::set<const IFlowNode*> Nodes;
    for (const auto& Step : InDefinition.Steps)
    {
        if (Step.Id.empty() || !Step.Node || !Steps.emplace(Step.Id, &Step).second ||
            !Nodes.insert(Step.Node.get()).second)
        {
            Error = "节点ID为空、重复，或节点实例缺失／复用";
            return false;
        }
        if (!std::isfinite(Step.TimeoutSeconds) || Step.TimeoutSeconds <= 0 ||
            !std::isfinite(Step.RetryDelaySeconds) || Step.RetryDelaySeconds < 0 || Step.MaxAttempts < 1)
        {
            Error = "节点超时、退避或尝试次数非法";
            return false;
        }
    }
    if (Steps.find(InDefinition.Entry) == Steps.end())
    {
        Error = "流程入口缺失";
        return false;
    }
    std::map<std::string, std::vector<std::string>> Edges;
    std::map<std::string, std::size_t> InDegree;
    for (const auto& Pair : Steps) InDegree[Pair.first] = 0;
    for (const auto& Step : InDefinition.Steps)
    {
        auto& Targets = Edges[Step.Id];
        if (!Step.Next.empty()) Targets.push_back(Step.Next);
        for (const auto& Route : Step.Routes)
        {
            if (Route.first.empty()) { Error = "具名分支不能使用空名称"; return false; }
            if (!Route.second.empty()) Targets.push_back(Route.second);
        }
        for (const auto& Target : Targets)
        {
            if (Steps.find(Target) == Steps.end()) { Error = "分支引用不存在的节点"; return false; }
            ++InDegree[Target];
        }
    }
    // 迭代拓扑检查避免深图递归耗尽栈；禁止用图循环实现无界重试。
    std::vector<std::string> Ready;
    for (const auto& Pair : InDegree) if (Pair.second == 0) Ready.push_back(Pair.first);
    for (std::size_t Cursor = 0; Cursor < Ready.size(); ++Cursor)
        for (const auto& Target : Edges[Ready[Cursor]]) if (--InDegree[Target] == 0) Ready.push_back(Target);
    if (Ready.size() != Steps.size()) { Error = "流程图存在循环"; return false; }
    std::set<std::string> Reachable{InDefinition.Entry};
    std::vector<std::string> Pending{InDefinition.Entry};
    for (std::size_t Cursor = 0; Cursor < Pending.size(); ++Cursor)
        for (const auto& Target : Edges[Pending[Cursor]])
            if (Reachable.insert(Target).second) Pending.push_back(Target);
    if (Reachable.size() != Steps.size()) { Error = "流程包含入口不可达节点"; return false; }
    return true;
}

bool FApplicationFlowExecutor::Configure(FDefinition InDefinition, std::string& Error)
{
    if (!CheckControl(Error)) return false;
    if (IsActive()) { Error = "运行期间禁止替换流程图"; return false; }
    if (!Validate(InDefinition, Error)) return false;
    InvalidateCompletion();
    Definition = std::move(InDefinition);
    Index.clear();
    for (std::size_t I = 0; I < Definition.Steps.size(); ++I) Index.emplace(Definition.Steps[I].Id, I);
    Snapshot = {};
    return true;
}

std::uint64_t FApplicationFlowExecutor::Start(double NowSeconds, std::string& Error)
{
    if (!CheckControl(Error)) return 0;
    if (IsActive() || Index.empty() || !std::isfinite(NowSeconds) || NowSeconds < 0 ||
        LastRunId == std::numeric_limits<std::uint64_t>::max())
    {
        Error = "未配置、已运行、时间非法或流程代次耗尽";
        return 0;
    }
    Snapshot = {};
    Snapshot.State = EFlowState::Running;
    Snapshot.RunId = ++LastRunId;
    CurrentIndex = Index.at(Definition.Entry);
    Snapshot.NodeId = Definition.Entry;
    LastNowSeconds = NowSeconds;
    bNeedsBegin = true;
    return Snapshot.RunId;
}

void FApplicationFlowExecutor::InvalidateCompletion()
{
    std::lock_guard<std::mutex> Lock(Mailbox->Mutex);
    Mailbox->bAccepting = false;
    Mailbox->Result.reset();
}

void FApplicationFlowExecutor::BeginAttempt(double NowSeconds)
{
    if (LastToken == std::numeric_limits<std::uint64_t>::max())
    {
        Fail("AttemptGenerationExhausted", "节点尝试代次耗尽");
        return;
    }
    auto& Step = Definition.Steps[CurrentIndex];
    DeadlineSeconds = NowSeconds + Step.TimeoutSeconds;
    if (!std::isfinite(DeadlineSeconds)) { Fail("InvalidDeadline", "节点截止时间溢出"); return; }
    const auto Token = ++LastToken;
    {
        std::lock_guard<std::mutex> Lock(Mailbox->Mutex);
        Mailbox->Token = Token;
        Mailbox->bAccepting = true;
        Mailbox->Result.reset();
    }
    ++Snapshot.Attempt;
    Snapshot.State = EFlowState::Running;
    bNeedsBegin = false;
    bAttemptActive = true;
    const std::weak_ptr<FMailbox> WeakMailbox = Mailbox;
    bInNodeCallback = true;
    Step.Node->Start({Snapshot.RunId, Step.Id, Snapshot.Attempt}, [WeakMailbox, Token](FNodeResult Result)
    {
        if (auto Inbox = WeakMailbox.lock())
        {
            std::lock_guard<std::mutex> Lock(Inbox->Mutex);
            if (Inbox->bAccepting && Inbox->Token == Token)
            {
                Inbox->Result = std::move(Result);
                Inbox->bAccepting = false; // 同一尝试只有首次完成可投递。
            }
        }
    });
    bInNodeCallback = false;
}

void FApplicationFlowExecutor::EndAttempt(EFinishReason Reason)
{
    InvalidateCompletion();
    if (!bAttemptActive) return;
    bAttemptActive = false;
    bInNodeCallback = true;
    Definition.Steps[CurrentIndex].Node->Finish(Reason);
    bInNodeCallback = false;
}

void FApplicationFlowExecutor::Fail(std::string Code, std::string Message)
{
    EndAttempt(EFinishReason::Failed);
    bNeedsBegin = false;
    Snapshot.State = EFlowState::Failed;
    Snapshot.ErrorCode = std::move(Code);
    Snapshot.ErrorMessage = std::move(Message);
}

void FApplicationFlowExecutor::HandleFailure(const FNodeResult& Result, bool bTimedOut, double NowSeconds)
{
    EndAttempt(bTimedOut ? EFinishReason::TimedOut : EFinishReason::Failed);
    const auto& Step = Definition.Steps[CurrentIndex];
    if ((bTimedOut ? Step.bRetryOnTimeout : Result.bRetryable) && Snapshot.Attempt < Step.MaxAttempts)
    {
        RetryAtSeconds = NowSeconds + Step.RetryDelaySeconds;
        if (!std::isfinite(RetryAtSeconds)) { Fail("InvalidRetryTime", "重试时间溢出"); return; }
        Snapshot.State = EFlowState::RetryWaiting;
        return;
    }
    Fail(bTimedOut ? "NodeTimedOut" : (Result.ErrorCode.empty() ? "NodeFailed" : Result.ErrorCode),
        bTimedOut ? "节点超过截止时间" : Result.ErrorMessage);
}

void FApplicationFlowExecutor::Tick(double NowSeconds)
{
    std::string Error;
    if (!CheckControl(Error) || !IsActive()) return;
    if (!std::isfinite(NowSeconds) || NowSeconds < LastNowSeconds)
    {
        Fail("InvalidClock", "流程要求有限、单调递增的时间");
        return;
    }
    LastNowSeconds = NowSeconds;
    if (Snapshot.State == EFlowState::RetryWaiting)
    {
        if (NowSeconds >= RetryAtSeconds) BeginAttempt(NowSeconds);
        return;
    }
    if (bNeedsBegin) { BeginAttempt(NowSeconds); return; }
    if (NowSeconds >= DeadlineSeconds) { HandleFailure({}, true, NowSeconds); return; }
    std::optional<FNodeResult> Result;
    {
        std::lock_guard<std::mutex> Lock(Mailbox->Mutex);
        Result = std::move(Mailbox->Result);
        Mailbox->Result.reset();
    }
    if (!Result) return;
    if (!Result->bSucceeded) { HandleFailure(*Result, false, NowSeconds); return; }
    const auto& Step = Definition.Steps[CurrentIndex];
    std::string Next = Step.Next;
    if (!Result->Outcome.empty())
    {
        auto Route = Step.Routes.find(Result->Outcome);
        if (Route == Step.Routes.end()) { Fail("UnknownOutcome", "节点返回未声明的分支"); return; }
        Next = Route->second;
    }
    EndAttempt(EFinishReason::Succeeded);
    if (Next.empty()) { Snapshot.State = EFlowState::Succeeded; return; }
    CurrentIndex = Index.at(Next);
    Snapshot.NodeId = Next;
    Snapshot.Attempt = 0;
    bNeedsBegin = true;
}

bool FApplicationFlowExecutor::Cancel(std::uint64_t RunId)
{
    std::string Error;
    if (!CheckControl(Error) || !IsActive() || Snapshot.RunId != RunId) return false;
    EndAttempt(EFinishReason::Cancelled);
    bNeedsBegin = false;
    Snapshot.State = EFlowState::Cancelled;
    Snapshot.ErrorCode = "Cancelled";
    Snapshot.ErrorMessage = "调用方取消流程";
    return true;
}

bool FApplicationFlowExecutor::Shutdown()
{
    std::string Error;
    if (!CheckControl(Error)) return false;
    EndAttempt(EFinishReason::Shutdown);
    bNeedsBegin = false;
    Snapshot.State = EFlowState::Shutdown;
    Snapshot.ErrorCode = "Shutdown";
    Snapshot.ErrorMessage = "流程作用域关闭";
    Definition = {};
    Index.clear();
    return true;
}
}
