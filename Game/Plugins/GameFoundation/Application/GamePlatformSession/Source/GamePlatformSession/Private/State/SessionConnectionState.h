#pragma once

#include <cstdint>
#include <string>

// 纯状态内核由游戏线程串行调用；没有网络、UObject、身份验证或凭据所有权。
namespace GamePlatformSession
{
enum class EState { Idle, RequestingAssignment, PreparingConnection, Connecting, AwaitingAdmission, Ready, Transferring, Reconnecting, Leaving };
enum class EIntent { Join, Transfer, Reconnect };
enum class EFact { NetworkConnected, AdmissionConfirmed, TargetWorldLoaded, ControllerReady };
enum class EOutcome { None, Succeeded, Cancelled, Failed, TimedOut, Uncertain, AuthChanged };
enum class EAcceptance { Accepted, Busy, Invalid, Stale, NotAuthenticated, WrongState };

// 非凭据的关联身份。账号刷新不改变AuthGeneration；退出/换账号必须递增。
struct FOperationIdentity
{
    std::string ScopeId;
    std::string OperationId;
    std::string AttemptId;
    std::uint64_t AuthGeneration = 0;
    std::uint64_t ConnectionGeneration = 0;
    bool operator==(const FOperationIdentity& Other) const;
};

// 来自已验证后端响应的非敏感描述；内核不接受端点或原始票据，不承担验证签名。
struct FBinding
{
    std::string AssignmentId;
    std::string GameSessionId;
    std::string ServerInstanceId;
    std::string ServerBootId;
    std::string WorldId;
    std::string ProtocolVersion;
    std::uint64_t SessionEpoch = 0;
    bool IsValid() const;
    bool operator==(const FBinding& Other) const;
};

// 值快照；Current与Pending严格分离，复制给诊断方不能修改内核。
struct FSnapshot
{
    EState State = EState::Idle;
    FBinding Current;
    FBinding Pending;
    FOperationIdentity Operation;
    EOutcome LastOutcome = EOutcome::None;
    bool bOperationActive = false;
    bool bRemoteResolutionRequired = false;
    std::uint64_t CompletionCount = 0;
};

class FSessionConnectionState
{
public:
    // ScopeId由每个游戏实例独立创建；空身份导致所有Begin失败。
    explicit FSessionConnectionState(std::string ScopeId);
    // AuthSessionId不是令牌。更新同代次不同账号会拒绝；换账号清空旧连接及操作。
    EAcceptance SetAuthentication(std::string AuthSessionId, std::uint64_t Generation);
    // 同操作身份重试返回已有句柄；截止时间为调用者单调时钟秒，不使用世界时间。
    EAcceptance Begin(EIntent Intent, std::string OperationId, std::string AttemptId,
        double NowSeconds, double DeadlineSeconds, FOperationIdentity& OutIdentity);
    // 仅接收经过授权/版本验证的完整目标描述；不将Pending设置为Current。
    EAcceptance Assign(const FOperationIdentity& Identity, const FBinding& Binding, double NowSeconds);
    // 必须在调用引擎旅行之前记录该不可撤回边界；迟到取消随后返回Uncertain。
    EAcceptance CommitTravel(const FOperationIdentity& Identity, double NowSeconds);
    // 由可信适配器关联真实驱动/控制器后报告事实，四事实全部满足才完成。此API不是可信网络入口。
    EAcceptance Observe(const FOperationIdentity& Identity, const FBinding& Binding, EFact Fact, double NowSeconds);
    // 未切服且来源仍经调用者确认为有效时保留来源；远端撤销仍需独立确认。
    EAcceptance Cancel(const FOperationIdentity& Identity, double NowSeconds);
    EAcceptance Fail(const FOperationIdentity& Identity, double NowSeconds);
    // 后端查询确认取消/释放后清除本次不确定屏障；空Binding表示已确认无绑定。
    EAcceptance ResolveRemote(const FOperationIdentity& Identity, const FBinding& ConfirmedBinding);
    // 外部实例计时器调用；只完成一次，不轮询网络、不延长重试总预算。
    void AdvanceDeadline(double NowSeconds);
    // 仅匹配当前绑定/代次才清空；旧来源迟到断线不能影响新目标。
    bool Disconnect(const FBinding& Binding);
    // 返回值只确认本地状态清理；远端释放结果由实际后端适配器单独确认。
    void Leave();
    FSnapshot Snapshot() const { return View; }

private:
    EAcceptance Check(const FOperationIdentity& Identity, double NowSeconds);
    void Finish(EOutcome Outcome, bool bKeepSource, bool bNeedsResolution);
    FSnapshot View;
    std::string Scope;
    std::string AuthSession;
    std::uint64_t AuthGeneration = 0;
    std::uint64_t ConnectionGeneration = 0;
    EIntent ActiveIntent = EIntent::Join;
    double Deadline = 0;
    unsigned int Facts = 0;
    bool bTravelCommitted = false;
};
}
