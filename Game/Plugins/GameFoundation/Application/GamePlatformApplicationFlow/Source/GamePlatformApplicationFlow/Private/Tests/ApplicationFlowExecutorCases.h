#pragma once

#include "Execution/ApplicationFlowExecutor.h"
#include <limits>
#include <utility>

namespace GamePlatform::ApplicationFlow::Tests
{
/** 同一组行为断言用于本机 C++ 测试和 UE Automation；测试替身仅存在于测试目录。 */
struct FChecks
{
    std::vector<std::string> Failures;
    void Require(bool bCondition, const char* Message) { if (!bCondition) Failures.emplace_back(Message); }
};

struct FTestNode final : IFlowNode
{
    int Starts = 0;
    std::vector<EFinishReason> Finishes;
    std::vector<FCompletion> Completions;
    std::function<void(const FExecutionContext&, FCompletion)> OnStart;
    std::function<void()> OnFinish;
    void Start(const FExecutionContext& Context, FCompletion Complete) override
    {
        ++Starts;
        Completions.push_back(Complete);
        if (OnStart) OnStart(Context, std::move(Complete));
    }
    void Finish(EFinishReason Reason) override
    {
        Finishes.push_back(Reason);
        if (OnFinish) OnFinish();
    }
};

inline FNodeResult Success(std::string Outcome = {}) { return {true, false, std::move(Outcome), {}, {}}; }
inline FNodeResult Failure(bool bRetryable = false) { return {false, bRetryable, {}, "Unavailable", "服务暂不可用"}; }
inline FStep Step(std::string Id, const std::shared_ptr<FTestNode>& Node, std::string Next = {})
{
    FStep Value;
    Value.Id = std::move(Id);
    Value.Node = Node;
    Value.Next = std::move(Next);
    return Value;
}

struct FCase
{
    const char* Name;
    std::function<void(FChecks&)> Run;
};

inline std::vector<FCase> GetCases()
{
    return {
        {"SequentialAndSynchronousCompletion", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto B = std::make_shared<FTestNode>();
            A->OnStart = B->OnStart = [](const auto&, auto Done) { Done(Success()); };
            FApplicationFlowExecutor E; std::string Error;
            C.Require(E.Configure({"a", {Step("a", A, "b"), Step("b", B)}}, Error), "合法顺序图应安装");
            C.Require(E.Start(0, Error) != 0 && A->Starts == 0, "Start 不同步进入节点");
            E.Tick(0); C.Require(A->Starts == 1 && B->Starts == 0, "同步完成不递归启动后续节点");
            E.Tick(1); E.Tick(2); E.Tick(3);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "顺序流程应成功");
            C.Require(A->Finishes.size() == 1 && B->Finishes.size() == 1, "每次成功尝试均须清理");
        }},
        {"NamedBranchSkipsDefault", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto B = std::make_shared<FTestNode>(); auto D = std::make_shared<FTestNode>();
            auto Entry = Step("a", A, "b"); Entry.Routes["optional"] = "d";
            FApplicationFlowExecutor E; std::string Error;
            C.Require(E.Configure({"a", {Entry, Step("b", B), Step("d", D)}}, Error), "合法分支图应安装");
            E.Start(0, Error); E.Tick(0); A->Completions[0](Success("optional")); E.Tick(1); E.Tick(2);
            C.Require(B->Starts == 0 && D->Starts == 1, "具名分支不能进入默认边");
            D->Completions[0](Success()); E.Tick(3);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "所选分支完成");
        }},
        {"UnknownOutcomeFails", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Success("undeclared")); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Failed && E.GetSnapshot().ErrorCode == "UnknownOutcome", "未知分支必须失败");
            C.Require(A->Finishes.size() == 1 && A->Finishes[0] == EFinishReason::Failed, "非法结果仍清理一次");
        }},
        {"FailurePreservesError", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Failure()); E.Tick(1);
            C.Require(E.GetSnapshot().ErrorCode == "Unavailable" && E.GetSnapshot().ErrorMessage == "服务暂不可用", "错误码与中文诊断应保留");
            C.Require(A->Starts == 1 && A->Finishes.size() == 1, "非重试错误只执行一次");
        }},
        {"CancelAndLateCompletion", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); auto Id = E.Start(0, Error); E.Tick(0);
            C.Require(E.Cancel(Id), "当前流程取消成功"); C.Require(!E.Cancel(Id), "重复取消拒绝");
            A->Completions[0](Success()); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Cancelled && A->Finishes.size() == 1, "迟到完成不能复活已取消流程");
        }},
        {"CancelBeforeFirstTick", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); auto Id = E.Start(0, Error); E.Cancel(Id); E.Tick(0);
            C.Require(A->Starts == 0 && A->Finishes.empty(), "尚未开始的尝试不执行也不清理");
        }},
        {"OldRunCannotAffectRestart", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); auto Old = E.Start(0, Error); E.Tick(0); E.Cancel(Old);
            auto New = E.Start(1, Error); E.Tick(1); A->Completions[0](Success()); E.Tick(2);
            C.Require(New != Old && !E.Cancel(Old) && E.IsActive(), "旧句柄与旧完成不能操作新运行");
            A->Completions[1](Success()); E.Tick(3); C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "新运行自己的完成有效");
        }},
        {"DuplicateCompletionFirstWins", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Failure()); A->Completions[0](Success()); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Failed && A->Finishes.size() == 1, "只接收第一次完成");
        }},
        {"DeadlineBeatsQueuedCompletion", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A); S.TimeoutSeconds = 2;
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {S}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Success()); E.Tick(2);
            C.Require(E.GetSnapshot().ErrorCode == "NodeTimedOut", "到达截止时间后超时优先");
            C.Require(A->Finishes.size() == 1 && A->Finishes[0] == EFinishReason::TimedOut, "超时清理原因正确");
        }},
        {"RetryDelayAndOldAttemptIsolation", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A); S.MaxAttempts = 2; S.RetryDelaySeconds = 3;
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {S}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Failure(true)); E.Tick(1); E.Tick(3);
            C.Require(E.GetSnapshot().State == EFlowState::RetryWaiting && A->Starts == 1, "退避时间未到不得重试");
            E.Tick(4); A->Completions[0](Success()); E.Tick(5);
            C.Require(E.IsActive() && E.GetSnapshot().Attempt == 2, "旧尝试不能结束第二次尝试");
            A->Completions[1](Success()); E.Tick(6);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded && A->Finishes.size() == 2, "重试成功且两次都已清理");
        }},
        {"RetryBudgetExhausted", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A); S.MaxAttempts = 2;
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {S}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Failure(true)); E.Tick(1); E.Tick(2); A->Completions[1](Failure(true)); E.Tick(3); E.Tick(4);
            C.Require(A->Starts == 2 && E.GetSnapshot().State == EFlowState::Failed, "尝试预算耗尽必须终止");
        }},
        {"TimeoutRetryAndCancelBackoff", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A); S.TimeoutSeconds = 1; S.MaxAttempts = 2; S.bRetryOnTimeout = true;
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {S}}, Error); auto Id = E.Start(0, Error); E.Tick(0); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::RetryWaiting, "允许超时重试时进入退避");
            E.Cancel(Id); E.Tick(2);
            C.Require(A->Starts == 1 && A->Finishes.size() == 1 && !E.IsActive(), "退避期间取消不重复清理或启动节点");
        }},
        {"InvalidGraphsAreAtomic", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto B = std::make_shared<FTestNode>();
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {Step("a", A)}}, Error);
            std::vector<FDefinition> Bad = {
                {}, {"missing", {Step("a", A)}}, {"a", {Step("a", A), Step("a", B)}},
                {"a", {Step("a", A, "unknown")}}, {"a", {Step("a", A, "b"), Step("b", B, "a")}},
                {"a", {Step("a", A), Step("b", B)}}, {"a", {Step("a", A, "b"), Step("b", A)}}
            };
            for (auto& D : Bad) C.Require(!E.Configure(D, Error) && !Error.empty(), "非法图必须拒绝并返回诊断");
            C.Require(E.Start(0, Error) != 0, "非法安装后旧图仍有效"); E.Tick(0);
            C.Require(A->Starts == 1, "执行的仍是旧配置节点");
        }},
        {"InvalidLimitsRejected", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            auto S = Step("a", A); S.TimeoutSeconds = 0;
            C.Require(!E.Configure({"a", {S}}, Error), "零超时拒绝"); S.TimeoutSeconds = std::numeric_limits<double>::infinity();
            C.Require(!E.Configure({"a", {S}}, Error), "无限超时拒绝"); S.TimeoutSeconds = 1; S.MaxAttempts = 0;
            C.Require(!E.Configure({"a", {S}}, Error), "零次尝试拒绝"); S.MaxAttempts = 1; S.RetryDelaySeconds = -1;
            C.Require(!E.Configure({"a", {S}}, Error), "负退避拒绝"); S.RetryDelaySeconds = 0; S.Routes[""] = "";
            C.Require(!E.Configure({"a", {S}}, Error), "空分支名拒绝");
        }},
        {"BusyAndReentrantControlRejected", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            A->OnStart = [&](const auto& Context, auto)
            {
                C.Require(!E.Cancel(Context.RunId), "节点回调不允许重入取消");
                C.Require(E.Start(0, Error) == 0, "节点回调不允许重入启动");
                C.Require(!E.Shutdown(), "节点回调不允许重入关闭");
            };
            E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
            C.Require(!E.Configure({"a", {Step("a", A)}}, Error), "运行中不得换图");
        }},
        {"ThreadedCompletionAndControl", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); auto Id = E.Start(0, Error); E.Tick(0);
            bool bCancelled = true;
            std::thread Worker([&] { bCancelled = E.Cancel(Id); A->Completions[0](Success()); }); Worker.join();
            C.Require(!bCancelled && E.IsActive(), "工作线程只投递完成，不控制状态"); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "跨线程完成在所有者线程消费");
        }},
        {"ConcurrentDuplicateCompletions", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
            std::vector<std::thread> Workers;
            for (int I = 0; I < 16; ++I) Workers.emplace_back([&] { A->Completions[0](Success()); });
            for (auto& Worker : Workers) Worker.join(); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded && A->Finishes.size() == 1, "并发重复完成只清理一次");
        }},
        {"IndependentScopes", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto B = std::make_shared<FTestNode>();
            FApplicationFlowExecutor Left, Right; std::string Error;
            Left.Configure({"a", {Step("a", A)}}, Error); Right.Configure({"b", {Step("b", B)}}, Error);
            auto L = Left.Start(0, Error); Right.Start(0, Error); Left.Tick(0); Right.Tick(0); Left.Cancel(L);
            B->Completions[0](Success()); Right.Tick(1);
            C.Require(Left.GetSnapshot().State == EFlowState::Cancelled && Right.GetSnapshot().State == EFlowState::Succeeded, "多实例流程状态隔离");
        }},
        {"ShutdownAndCallbackAfterDestruction", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); std::string Error;
            {
                FApplicationFlowExecutor E; E.Configure({"a", {Step("a", A)}}, Error); E.Start(0, Error); E.Tick(0);
                C.Require(E.Shutdown(), "首次关闭成功"); C.Require(!E.Shutdown() && E.Start(1, Error) == 0, "重复关闭／关闭后启动拒绝");
            }
            A->Completions[0](Success());
            C.Require(A->Finishes.size() == 1 && A->Finishes[0] == EFinishReason::Shutdown, "析构后迟到回调安全，关闭清理一次");
        }},
        {"InvalidClockFailsAndCleansUp", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error);
            C.Require(E.Start(std::numeric_limits<double>::quiet_NaN(), Error) == 0, "NaN 启动时间拒绝");
            E.Start(2, Error); E.Tick(2); E.Tick(1);
            C.Require(E.GetSnapshot().ErrorCode == "InvalidClock" && A->Finishes.size() == 1, "时钟回退终止并清理");
        }},
        {"ExplicitCycleOptInAndGeneration", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>();
            std::vector<std::uint64_t> Generations;
            A->OnStart = [&](const auto& Context, auto) { Generations.push_back(Context.NodeGeneration); };
            auto S = Step("a", A, "a"); S.Routes["done"] = "";
            FDefinition D{"a", {S}}; FApplicationFlowExecutor E; std::string Error;
            C.Require(!E.Configure(D, Error), "旧默认仍拒绝循环");
            D.bAllowCycles = true;
            const bool bConfigured = E.Configure(D, Error);
            C.Require(bConfigured, "显式资产模式应允许循环"); if (!bConfigured) return;
            E.Start(0, Error); E.Tick(0);
            C.Require(Generations[0] != 0 && E.GetSnapshot().NodeGeneration == Generations[0], "上下文和快照暴露同一非零节点代次");
            A->Completions[0](Success()); E.Tick(1); E.Tick(2);
            C.Require(Generations.size() == 2 && Generations[0] != Generations[1], "重访同名节点必须新代次");
            A->Completions[0](Success("done")); E.Tick(3);
            C.Require(E.IsActive(), "旧回调不能结束重访节点");
            A->Completions[1](Success("done")); E.Tick(4);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded && A->Finishes.size() == 2, "异步循环可明确退出且每次清理");
        }},
        {"ImmediateCycleBudgetFails", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); A->OnStart = [](const auto&, auto Done) { Done(Success()); };
            FDefinition D{"a", {Step("a", A, "a")}}; D.bAllowCycles = true; D.MaxImmediateCycleTransitions = 3;
            FApplicationFlowExecutor E; std::string Error;
            const bool bConfigured = E.Configure(D, Error); C.Require(bConfigured, "循环预算图可安装"); if (!bConfigured) return;
            E.Start(0, Error); for (int I = 0; I < 20 && E.IsActive(); ++I) E.Tick(I);
            C.Require(E.GetSnapshot().State == EFlowState::Failed && E.GetSnapshot().ErrorCode == "ImmediateCycleBudgetExceeded", "即时自环预算耗尽真实失败");
            C.Require(A->Starts == 4 && A->Finishes.size() == 4, "允许三次重访并清理所有已启动节点");
        }},
        {"AsynchronousWaitResetsCycleBudget", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>();
            auto S = Step("a", A, "a"); S.Routes["done"] = "";
            FDefinition D{"a", {S}}; D.bAllowCycles = true; D.MaxImmediateCycleTransitions = 1;
            FApplicationFlowExecutor E; std::string Error;
            const bool bConfigured = E.Configure(D, Error); C.Require(bConfigured, "等待循环图可安装"); if (!bConfigured) return;
            E.Start(0, Error);
            for (int I = 0; I < 8; ++I) { E.Tick(I * 2); A->Completions.back()(Success(I == 7 ? "done" : "")); E.Tick(I * 2 + 1); }
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded && A->Starts == 8, "真实等待后重新计数，不限制用户多次交互");
        }},
        {"CycleModeStillValidatesGraph", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto B = std::make_shared<FTestNode>();
            FDefinition D{"a", {Step("a", A, "a")}}; D.bAllowCycles = true; D.MaxImmediateCycleTransitions = 0;
            FApplicationFlowExecutor E; std::string Error;
            C.Require(!E.Configure(D, Error), "循环模式零预算无效"); D.MaxImmediateCycleTransitions = 1;
            D.Steps.push_back(Step("b", B)); C.Require(!E.Configure(D, Error), "循环模式仍拒绝不可达节点");
            D.Steps[0].Next = "missing"; C.Require(!E.Configure(D, Error), "循环模式仍拒绝悬空边");
        }},
        {"GenerationChangesAcrossRetry", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A); S.MaxAttempts = 2;
            std::vector<std::uint64_t> Generations;
            A->OnStart = [&](const auto& Context, auto) { Generations.push_back(Context.NodeGeneration); };
            FApplicationFlowExecutor E; std::string Error; E.Configure({"a", {S}}, Error); E.Start(0, Error); E.Tick(0);
            A->Completions[0](Failure(true)); E.Tick(1); E.Tick(2);
            C.Require(Generations.size() == 2 && Generations[0] != 0 && Generations[1] > Generations[0], "重试也必须改变公开节点代次");
        }},
        {"ExternalEventUsesAttemptMailbox", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); auto S = Step("a", A, "a"); S.Routes["done"] = "";
            FDefinition D{"a", {S}}; D.bAllowCycles = true;
            FApplicationFlowExecutor E; std::string Error; E.Configure(D, Error); const auto Run = E.Start(0, Error);
            C.Require(!E.SubmitEvent(Run, "a", 1, Success()), "尚未开始不能投递事件"); E.Tick(0);
            const auto First = E.GetSnapshot().NodeGeneration;
            C.Require(!E.SubmitEvent(Run + 1, "a", First, Success()), "错误运行拒绝");
            C.Require(!E.SubmitEvent(Run, "other", First, Success()), "错误节点拒绝");
            C.Require(E.SubmitEvent(Run, "a", First, Success()), "精确事件进入邮箱");
            C.Require(!E.SubmitEvent(Run, "a", First, Success("done")), "同代次只接纳首次完成");
            A->Completions[0](Success("done")); E.Tick(1); E.Tick(2);
            const auto Second = E.GetSnapshot().NodeGeneration;
            C.Require(Second != First && E.IsActive(), "外部事件与回调共享首次完成语义");
            C.Require(!E.SubmitEvent(Run, "a", First, Success("done")) && !E.CancelNode(Run, "a", First), "循环旧令牌不能投递或取消");
            A->Completions[1](Success("done"));
            C.Require(!E.SubmitEvent(Run, "a", Second, Failure()), "回调先到时外部事件拒绝"); E.Tick(3);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "首次完成结果未被覆盖");
        }},
        {"ExactCancellationAndThreadGuards", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            A->OnStart = [&](const auto& Context, auto)
            {
                C.Require(!E.SubmitEvent(Context.RunId, Context.NodeId, Context.NodeGeneration, Success()), "Execute中不允许控制重入");
            };
            E.Configure({"a", {Step("a", A)}}, Error); const auto Run = E.Start(0, Error); E.Tick(0);
            const auto Generation = E.GetSnapshot().NodeGeneration;
            bool bEvent = true; bool bCancel = true;
            std::thread Worker([&] { bEvent = E.SubmitEvent(Run, "a", Generation, Success()); bCancel = E.CancelNode(Run, "a", Generation); }); Worker.join();
            C.Require(!bEvent && !bCancel, "外部事件与精确取消控制限所有者线程");
            C.Require(E.CancelNode(Run, "a", Generation), "正确令牌可以取消");
            C.Require(!E.CancelNode(Run, "a", Generation) && !E.SubmitEvent(Run, "a", Generation, Success()), "终态后事件和取消不再接纳");
            C.Require(A->Finishes.size() == 1, "取消只清理一次");
        }},
        {"GraphPreflightAndAtomicStart", [](FChecks& C)
        {
            FDefinition Graph; Graph.Entry = "a"; FStep Entry; Entry.Id = "a"; Graph.Steps.push_back(Entry);
            std::string Error; C.Require(FApplicationFlowExecutor::ValidateGraph(Graph, Error), "结构预检无需伪造节点实例");
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E;
            E.Configure({"old", {Step("old", A)}}, Error);
            C.Require(E.ConfigureAndStart(Graph, 0, Error) == 0, "真实启动仍拒绝缺少实例");
            auto B = std::make_shared<FTestNode>(); FDefinition Other{"new", {Step("new", B)}};
            C.Require(E.ConfigureAndStart(Other, std::numeric_limits<double>::quiet_NaN(), Error) == 0, "非法时钟在替换前拒绝");
            C.Require(E.Start(0, Error) != 0, "原配置仍可启动"); E.Tick(0);
            C.Require(A->Starts == 1 && B->Starts == 0, "失败不会部分安装或执行工厂图");
        }},
        {"CycleModeLongChainNotBudgeted", [](FChecks& C)
        {
            FDefinition D; D.Entry = "0"; D.bAllowCycles = true; D.MaxImmediateCycleTransitions = 1;
            for (int I = 0; I < 2000; ++I)
            {
                auto N = std::make_shared<FTestNode>(); N->OnStart = [](const auto&, auto Done) { Done(Success()); };
                D.Steps.push_back(Step(std::to_string(I), N, I == 1999 ? "" : std::to_string(I + 1)));
            }
            FApplicationFlowExecutor E; std::string Error; E.ConfigureAndStart(std::move(D), 0, Error);
            for (int I = 0; I < 4000; ++I) E.Tick(I);
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "资产模式也不能把长无环链算成循环预算");
        }},
        {"DependencyLossFailsAndCleansUp", [](FChecks& C)
        {
            auto A = std::make_shared<FTestNode>(); FApplicationFlowExecutor E; std::string Error;
            E.Configure({"a", {Step("a", A)}}, Error); const auto Run = E.Start(0, Error); E.Tick(0);
            C.Require(!E.FailRun(Run + 1, "Expired", "旧运行"), "依赖失败也必须匹配当前运行");
            C.Require(E.FailRun(Run, "FlowLeaseExpired", "定义租约失效"), "宿主依赖失效真实失败");
            C.Require(E.GetSnapshot().State == EFlowState::Failed && E.GetSnapshot().ErrorCode == "FlowLeaseExpired", "不能把资源失效伪装成功或用户取消");
            C.Require(A->Finishes.size() == 1 && A->Finishes[0] == EFinishReason::Failed, "失败清理一次");
            A->Completions[0](Success()); E.Tick(1);
            C.Require(E.GetSnapshot().State == EFlowState::Failed && !E.FailRun(Run, "Again", "重复"), "迟到完成与重复失败无效");
        }},
        {"LongGraphDoesNotRecurse", [](FChecks& C)
        {
            FDefinition D; D.Entry = "0";
            for (int I = 0; I < 2000; ++I)
            {
                auto N = std::make_shared<FTestNode>(); N->OnStart = [](const auto&, auto Done) { Done(Success()); };
                D.Steps.push_back(Step(std::to_string(I), N, I == 1999 ? "" : std::to_string(I + 1)));
            }
            FApplicationFlowExecutor E; std::string Error;
            C.Require(E.Configure(std::move(D), Error), "深图可迭代验证"); E.Start(0, Error);
            for (int I = 0; I < 4000; ++I) E.Tick(static_cast<double>(I));
            C.Require(E.GetSnapshot().State == EFlowState::Succeeded, "长同步链在有限调度次数内完成，不递归");
        }}
    };
}
}
