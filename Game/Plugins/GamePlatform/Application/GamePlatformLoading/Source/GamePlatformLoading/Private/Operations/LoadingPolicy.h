#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 无引擎对象的生产状态算法；所有调用由实例门面在游戏线程串行化。
namespace GamePlatformLoadingPolicy
{
/**
 * Loading（加载）策略层安全上限。
 * 这些常量用于防止错误配置构造超大任务图造成主线程尖峰，不代表产品性能预算。
 * 若未来需要调整，必须同时更新原生测试、插件设计文档并以真实项目规模压测为依据。
 */
inline constexpr std::size_t MaxTasksPerOperation = 256;
inline constexpr std::size_t MaxDependenciesPerTask = 64;
enum class Requirement { Required, Optional, Degradable };
enum class TaskState { Waiting, Running, Succeeded, Failed, Cancelled, Degraded };
enum class OperationState { Idle, Running, Ready, DegradedReady, Failed, Cancelled, TimedOut };
/** Ticker调度策略：空闲完全停表，运行态主动采样，Ready资源保留态只做低频所有者监视。 */
enum class SamplingMode { Idle, Active, Retained };
/**
 * 纯状态决策，不读取引擎时钟；Subsystem只负责把Active/Retained映射到具体采样间隔。
 * Dirty/PendingSnapshot优先Active，确保外部状态变化尽快发布一次，然后可回到低频或Idle。
 */
inline SamplingMode SelectSamplingMode(OperationState State, bool bResourcesHeld, bool bDirty, bool bHasPendingSnapshot)
{
    if (State == OperationState::Running || bDirty || bHasPendingSnapshot) { return SamplingMode::Active; }
    if (bResourcesHeld) { return SamplingMode::Retained; }
    return SamplingMode::Idle;
}
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
        if (Specs.empty() || Specs.size() > MaxTasksPerOperation || !Generation || !std::isfinite(Now) ||
            !std::isfinite(Timeout) || Timeout <= 0 || !std::isfinite(Now + Timeout)) { return "InvalidOperation"; }

        // 启动校验只执行一次，但仍避免O(N²)字符串查找；大型任务图的运行期开销应集中在真实任务而非ID搜索。
        std::unordered_map<std::string, std::size_t> KnownIds;
        KnownIds.reserve(Specs.size());
        double WeightSum = 0;
        for (std::size_t Index = 0; Index < Specs.size(); ++Index)
        {
            const auto& Spec = Specs[Index];
            if (Spec.Id.empty() || Spec.Dependencies.size() > MaxDependenciesPerTask ||
                !std::isfinite(Spec.Weight) || Spec.Weight <= 0 ||
                !std::isfinite(Spec.TimeoutSeconds) || Spec.TimeoutSeconds <= 0 ||
                !std::isfinite(Now + Spec.TimeoutSeconds) ||
                (Spec.Requiredness == Requirement::Degradable && !Spec.HasFallback)) { return "InvalidTask"; }
            if (!KnownIds.emplace(Spec.Id, Index).second) { return "DuplicateTask"; }
            WeightSum += Spec.Weight;
        }
        if (!std::isfinite(WeightSum)) { return "InvalidWeightSum"; }

        for (const auto& Spec : Specs)
        {
            std::unordered_set<std::string> UniqueDependencies;
            UniqueDependencies.reserve(Spec.Dependencies.size());
            for (const auto& Dependency : Spec.Dependencies)
            {
                if (KnownIds.find(Dependency) == KnownIds.end()) { return "UnknownDependency"; }
                if (!UniqueDependencies.insert(Dependency).second) { return "DuplicateDependency"; }
            }
        }

        // 有界拓扑消除，不递归；Resolved使用哈希集合避免每层对已完成ID做线性扫描。
        std::unordered_set<std::string> Resolved;
        Resolved.reserve(Specs.size());
        while (Resolved.size() < Specs.size())
        {
            const auto Previous = Resolved.size();
            for (const auto& Spec : Specs)
            {
                if (Resolved.find(Spec.Id) != Resolved.end()) { continue; }
                if (std::all_of(Spec.Dependencies.begin(), Spec.Dependencies.end(), [&](const auto& Dependency)
                    { return Resolved.find(Dependency) != Resolved.end(); })) { Resolved.insert(Spec.Id); }
            }
            if (Previous == Resolved.size()) { return "DependencyCycle"; }
        }

        Tasks.clear();
        TaskIndex.clear();
        Tasks.reserve(Specs.size());
        TaskIndex.reserve(Specs.size());
        for (const auto& Spec : Specs)
        {
            TaskRecord Record; Record.Spec = Spec;
            TaskIndex.emplace(Spec.Id, Tasks.size());
            Tasks.push_back(std::move(Record));
        }
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
    /**
     * 启动后任务集合冻结，因此可以维护稳定的ID→数组索引。
     * 运行期依赖判定每50毫秒可能执行一次，使用索引避免反复线性扫描任务数组。
     */
    std::unordered_map<std::string, std::size_t> TaskIndex;
    TaskRecord* Find(const std::string& Id)
    {
        const auto Found = TaskIndex.find(Id);
        return Found != TaskIndex.end() && Found->second < Tasks.size() ? &Tasks[Found->second] : nullptr;
    }
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
