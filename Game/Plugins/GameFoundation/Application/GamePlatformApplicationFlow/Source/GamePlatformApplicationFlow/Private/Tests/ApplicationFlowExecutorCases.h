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
