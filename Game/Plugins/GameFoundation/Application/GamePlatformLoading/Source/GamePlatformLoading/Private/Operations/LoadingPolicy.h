#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// 无引擎对象的生产状态算法；所有调用由实例门面在游戏线程串行化。
namespace GamePlatformLoadingPolicy
{
enum class Requirement { Required, Optional, Degradable };
enum class TaskState { Waiting, Running, Succeeded, Failed, Cancelled, Degraded };
enum class OperationState { Idle, Running, Ready, DegradedReady, Failed, Cancelled, TimedOut };
struct TaskSpec
{
    std::string Id;
    Requirement Requiredness = Requirement::Required;
    std::vector<std::string> Dependencies;
    double Weight = 1;
    double TimeoutSeconds = 30;
    bool HasFallback = false;
};
struct ExecutionToken { std::uint64_t Operation = 0; std::string Id; std::uint64_t Generation = 0; };
struct TaskRecord
{
    TaskSpec Spec;
    TaskState State = TaskState::Waiting;
    double Progress = 0;
    double Deadline = 0;
    std::uint64_t Generation = 1;
    bool Fallback = false;
    std::string Error;
};
class Operation
{
public:
    OperationState State = OperationState::Idle;
    std::vector<TaskRecord> Tasks;
    std::string Start(const std::vector<TaskSpec>& Specs, std::uint64_t Generation, double Now, double Timeout)
    {
        if (State == OperationState::Running) { return "Busy"; }
        if (Specs.empty() || Specs.size() > 256 || !Generation || !std::isfinite(Now) ||
            !std::isfinite(Timeout) || Timeout <= 0 || !std::isfinite(Now + Timeout)) { return "InvalidOperation"; }
        std::vector<std::string> Visited;
        double WeightSum = 0;
        for (const auto& Spec : Specs)
        {
            if (Spec.Id.empty() || !std::isfinite(Spec.Weight) || Spec.Weight <= 0 ||
                !std::isfinite(Spec.TimeoutSeconds) || Spec.TimeoutSeconds <= 0 ||
                !std::isfinite(Now + Spec.TimeoutSeconds) ||
                (Spec.Requiredness == Requirement::Degradable && !Spec.HasFallback)) { return "InvalidTask"; }
            if (std::find(Visited.begin(),Visited.end(),Spec.Id) != Visited.end()) { return "DuplicateTask"; }
            Visited.push_back(Spec.Id);
            WeightSum += Spec.Weight;
        }
        if (!std::isfinite(WeightSum)) { return "InvalidWeightSum"; }
        for (const auto& Spec : Specs)
        {
            std::vector<std::string> Dependencies;
            for (const auto& Dependency : Spec.Dependencies)
            {
                if (std::find(Visited.begin(),Visited.end(),Dependency) == Visited.end()) { return "UnknownDependency"; }
                if (std::find(Dependencies.begin(),Dependencies.end(),Dependency) != Dependencies.end()) { return "DuplicateDependency"; }
                Dependencies.push_back(Dependency);
            }
        }
        Visited.clear();
        // 有界拓扑消除，不递归；未发生进展说明存在环。
        while (Visited.size() < Specs.size())
        {
            const auto Previous = Visited.size();
            for (const auto& Spec : Specs)
            {
                if (std::find(Visited.begin(),Visited.end(),Spec.Id) != Visited.end()) { continue; }
                if (std::all_of(Spec.Dependencies.begin(),Spec.Dependencies.end(),[&](const auto& Dependency)
                    { return std::find(Visited.begin(),Visited.end(),Dependency) != Visited.end(); })) { Visited.push_back(Spec.Id); }
            }
            if (Previous == Visited.size()) { return "DependencyCycle"; }
        }
        Tasks.clear();
        for (const auto& Spec : Specs) { TaskRecord Record; Record.Spec = Spec; Tasks.push_back(Record); }
        OperationGeneration = Generation; Deadline = Now + Timeout; State = OperationState::Running;
        return {};
    }
    std::vector<ExecutionToken> Startable(double Now)
    {
        std::vector<ExecutionToken> Result;
        if (State != OperationState::Running) { return Result; }
        for (auto& Task : Tasks)
        {
            if (Task.State != TaskState::Waiting) { continue; }
            bool DependenciesReady = true;
            for (const auto& Dependency : Task.Spec.Dependencies)
            {
                const auto* Parent = Find(Dependency);
                if (Parent->State == TaskState::Failed || Parent->State == TaskState::Cancelled)
                {
                    Task.State = TaskState::Failed; Task.Error = "DependencyFailed";
                    if (Task.Spec.Requiredness != Requirement::Optional) { Terminate(OperationState::Failed); return {}; }
                    break;
                }
                DependenciesReady &= Parent->State == TaskState::Succeeded || Parent->State == TaskState::Degraded;
            }
            if (Task.State == TaskState::Waiting && DependenciesReady)
            {
                Task.State = TaskState::Running; Task.Deadline = Now + Task.Spec.TimeoutSeconds;
                Result.push_back({OperationGeneration,Task.Spec.Id,Task.Generation});
            }
        }
        Evaluate(); return Result;
    }
    bool Report(const ExecutionToken& Token, double Progress)
    {
        auto* Task = Accept(Token);
        if (!Task || !std::isfinite(Progress)) { return false; }
        Task->Progress = std::max(Task->Progress,std::clamp(Progress,0.0,1.0)); return true;
    }
    bool Complete(const ExecutionToken& Token, bool Success, const std::string& Error)
    {
        auto* Task = Accept(Token); if (!Task) { return false; }
        if (Success) { Task->State = Task->Fallback ? TaskState::Degraded : TaskState::Succeeded; Task->Progress = 1; }
        else
        {
            Task->Error = Error;
            if (Task->Spec.Requiredness == Requirement::Degradable && !Task->Fallback)
            { Task->Fallback = true; ++Task->Generation; Task->State = TaskState::Waiting; }
            else
            {
                Task->State = TaskState::Failed;
                if (Task->Spec.Requiredness != Requirement::Optional) { Terminate(OperationState::Failed); }
            }
        }
        Evaluate(); return true;
    }
    void Cancel() { if (State == OperationState::Running) { Terminate(OperationState::Cancelled); } }
    void Expire(double Now)
    {
        if (State != OperationState::Running) { return; }
        if (Now >= Deadline) { Terminate(OperationState::TimedOut); return; }
        for (auto& Task : Tasks)
        {
            if (Task.State == TaskState::Running && Now >= Task.Deadline)
            { Complete({OperationGeneration,Task.Spec.Id,Task.Generation},false,"TaskTimeout"); }
        }
    }
    bool Ready() const { return State == OperationState::Ready || State == OperationState::DegradedReady; }
    double Progress() const
    {
        double Sum = 0, Weight = 0;
        for (const auto& Task : Tasks) { Sum += Task.Spec.Weight * Task.Progress; Weight += Task.Spec.Weight; }
        return Weight > 0 ? std::clamp(Sum / Weight,0.0,1.0) : 0;
    }
private:
    std::uint64_t OperationGeneration = 0;
    double Deadline = 0;
    TaskRecord* Find(const std::string& Id)
    { for (auto& Task : Tasks) { if (Task.Spec.Id == Id) { return &Task; } } return nullptr; }
    TaskRecord* Accept(const ExecutionToken& Token)
    {
        if (State != OperationState::Running || Token.Operation != OperationGeneration) { return nullptr; }
        auto* Task = Find(Token.Id);
        return Task && Task->Generation == Token.Generation && Task->State == TaskState::Running ? Task : nullptr;
    }
    void Terminate(OperationState Terminal)
    {
        State = Terminal;
        for (auto& Task : Tasks)
        { if (Task.State == TaskState::Running || Task.State == TaskState::Waiting) { Task.State = TaskState::Cancelled; } }
    }
    void Evaluate()
    {
        if (State != OperationState::Running) { return; }
        bool Degraded = false;
        for (const auto& Task : Tasks)
        {
            // 可选任务也等待有限终态，确保诊断完整；它的失败本身不否决Ready。
            if (Task.State == TaskState::Waiting || Task.State == TaskState::Running) { return; }
            Degraded |= Task.State == TaskState::Degraded;
        }
        State = Degraded ? OperationState::DegradedReady : OperationState::Ready;
    }
};
}
