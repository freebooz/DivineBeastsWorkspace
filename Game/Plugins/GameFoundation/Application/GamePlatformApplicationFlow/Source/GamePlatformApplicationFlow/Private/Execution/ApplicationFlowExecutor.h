#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace GamePlatform::ApplicationFlow
{
/** 内部状态机；不持有世界、账号或引擎全局状态。所有控制操作限构造线程。 */
enum class EFlowState { Idle, Running, RetryWaiting, Succeeded, Failed, Cancelled, Shutdown };
/** 每次尝试均调用一次 Finish，节点在其中解除本次委托、取消请求和释放本次资源。 */
enum class EFinishReason { Succeeded, Failed, Cancelled, TimedOut, Shutdown };

struct FNodeResult
{
    bool bSucceeded = false;
    bool bRetryable = false;
    std::string Outcome;       // 成功分支；空字符串表示默认边。
    std::string ErrorCode;     // 稳定错误码，不记录敏感载荷。
    std::string ErrorMessage;  // 面向诊断的脱敏中文说明。
};

struct FExecutionContext
{
    std::uint64_t RunId = 0;   // 当前执行器内的流程代次。
    std::string NodeId;
    int Attempt = 0;           // 从 1 开始，包括首次执行。
};

using FCompletion = std::function<void(FNodeResult)>;

/** Start/Finish 在所有者线程调用且不得阻塞。Completion 可在任意线程调用并可被迟到调用。 */
class IFlowNode
{
public:
    virtual ~IFlowNode() = default;
    virtual void Start(const FExecutionContext& Context, FCompletion Complete) = 0;
    virtual void Finish(EFinishReason Reason) = 0;
};

struct FStep
{
    std::string Id;
    std::shared_ptr<IFlowNode> Node;
    std::string Next;                          // 默认成功边；空表示流程成功结束。
    std::map<std::string, std::string> Routes;  // 非空 Outcome 到下一节点；空目标表示结束。
    double TimeoutSeconds = 30.0;             // 每次尝试的单调时钟截止时间，必须大于零。
    int MaxAttempts = 1;                       // 总尝试上限；大于 1 表示组合根承诺该节点可安全重试。
    double RetryDelaySeconds = 0.0;            // 重试固定退避时间；不自动扩大业务请求次数。
    bool bRetryOnTimeout = false;
};

struct FDefinition
{
    std::string Entry;
    std::vector<FStep> Steps;
};

struct FSnapshot
{
    EFlowState State = EFlowState::Idle;
    std::uint64_t RunId = 0;
    std::string NodeId;
    int Attempt = 0;
    std::string ErrorCode;
    std::string ErrorMessage;
};

/**
 * 单个应用主流程执行器。仅完成投递跨线程；其他方法包括只读 Snapshot 限所有者线程。
 * Tick 最多进入一个节点，避免同步完成形成递归或长帧；超时优先于同帧取出的完成结果。
 * 取消、重配、关闭通过失效邮箱代次拒绝旧回调，邮箱最多保留一个结果。
 */
class FApplicationFlowExecutor final
{
public:
    FApplicationFlowExecutor();
    ~FApplicationFlowExecutor();
    FApplicationFlowExecutor(const FApplicationFlowExecutor&) = delete;
    FApplicationFlowExecutor& operator=(const FApplicationFlowExecutor&) = delete;

    // 原子安装无环、全部可达的流程图。失败保留原配置；运行中或节点回调内拒绝变更。
    bool Configure(FDefinition Definition, std::string& Error);
    // 创建新代次；Start 不同步调用节点，节点在下次 Tick 执行。
    std::uint64_t Start(double NowSeconds, std::string& Error);
    // 使用单调时间处理一次完成、超时或退避；非法时间终止运行并记录错误。
    void Tick(double NowSeconds);
    // 仅匹配正在执行的 RunId 时生效；重复取消或旧句柄返回 false。
    bool Cancel(std::uint64_t RunId);
    // 关闭后不可再安装或启动；当前尝试先失效回调，再同步 Finish(Shutdown)。
    bool Shutdown();
    const FSnapshot& GetSnapshot() const { return Snapshot; }
    bool IsActive() const;

private:
    struct FMailbox
    {
        std::mutex Mutex;
        std::uint64_t Token = 0;
        bool bAccepting = false;
        std::optional<FNodeResult> Result;
    };
    bool CheckControl(std::string& Error) const;
    static bool Validate(const FDefinition& Definition, std::string& Error);
    void BeginAttempt(double NowSeconds);
    void EndAttempt(EFinishReason Reason);
    void InvalidateCompletion();
    void Fail(std::string Code, std::string Message);
    void HandleFailure(const FNodeResult& Result, bool bTimedOut, double NowSeconds);

    std::thread::id OwnerThread;
    std::shared_ptr<FMailbox> Mailbox;
    FDefinition Definition;
    std::map<std::string, std::size_t> Index;
    FSnapshot Snapshot;
    std::uint64_t LastRunId = 0;
    std::uint64_t LastToken = 0;
    std::size_t CurrentIndex = 0;
    bool bAttemptActive = false;
    bool bInNodeCallback = false;
    bool bNeedsBegin = false;
    double DeadlineSeconds = 0;
    double RetryAtSeconds = 0;
    double LastNowSeconds = 0;
};
}
