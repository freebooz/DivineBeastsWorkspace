#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformSessionClientSubsystem.generated.h"

/**
 * EGamePlatformSessionTransferState（平台客户端会话转移状态）。
 *
 * 这是只读公开投影，不是第二套网络权威状态机。真正的会话一致性仍由 GamePlatformSession
 * 私有状态内核维护；项目层只根据该状态显示界面或驱动 ApplicationFlow（应用流程）事件。
 */
UENUM(BlueprintType)
enum class EGamePlatformSessionTransferState : uint8
{
    Idle,
    RequestingAssignment,
    PreparingConnection,
    Connecting,
    AwaitingAdmission,
    Admitted,
    Ready,
    Transferring,
    Reconnecting,
    Failed,
    Cancelled,
    TimedOut,
    Uncertain
};

/** EGamePlatformSessionIntent（平台会话操作意图）；调用方必须显式区分首次加入、跨服与断线重连。 */
UENUM(BlueprintType)
enum class EGamePlatformSessionIntent : uint8
{
    Join,
    Transfer,
    Reconnect
};

/** EGamePlatformSessionRecoveryState（平台会话恢复状态）；避免项目层通过错误码猜测能否重试。 */
UENUM(BlueprintType)
enum class EGamePlatformSessionRecoveryState : uint8
{
    None,
    RetryAllowed,
    ReconciliationRequired,
    ReauthenticationRequired
};

/** EGamePlatformSessionTransferFact（平台会话可信事实）。 */
UENUM()
enum class EGamePlatformSessionTransferFact : uint8
{
    NetworkConnected,
    AdmissionConfirmed,
    TargetWorldLoaded,
    ControllerReady
};

/**
 * FGamePlatformSessionConnectionBinding（平台会话连接绑定）。
 *
 * 该结构只保存非敏感、可核对的连接身份。Endpoint（连接地址）和 TransferTicket（转移票据）
 * 不进入绑定快照，避免被 UI、日志或长期状态意外持有。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSESSION_API FGamePlatformSessionConnectionBinding
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString AssignmentId;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString GameSessionId;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString ServerInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString ServerBootId;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FName WorldId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString ProtocolVersion;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    int64 SessionEpoch = 0;

    /** 完整绑定必须来自已认证服务器握手或可信后端确认，客户端自报值不能作为权威绑定。 */
    bool IsValid() const;
};

/**
 * FGamePlatformSessionTransferRequest（平台会话转移请求）。
 *
 * TransferTicket 仅用于把一次后端授权交给真实 Transport（传输适配器）。
 * Session 子系统不会把票据复制到公开快照；调用方也不得记录、持久化或显示该字段。
 */
USTRUCT()
struct GAMEPLATFORMSESSION_API FGamePlatformSessionTransferRequest
{
    GENERATED_BODY()

    FGuid TransferOperationId;
    FString AssignmentId;
    FString GameServerId;
    FName ServerRoleId = NAME_None;
    FName ExperienceId = NAME_None;
    FName WorldId = NAME_None;
    FString Endpoint;
    FString TransferTicket;
    FString TicketId;
    /** 可选项目上下文；平台会话不会解释角色所有权，签票前应已由后端完成业务校验。 */
    FString CharacterId;
    FString SessionId;

    /**
     * 后端认证响应同时返回的完整非敏感目标Binding（连接绑定）。
     * Transport不得从Ticket正文、URL或客户端本地环境重新推导这些权威字段。
     */
    FGamePlatformSessionConnectionBinding ExpectedBinding;

    /** 整次连接/准入操作的单调时钟超时预算；不是 World 时间。 */
    double TimeoutSeconds = 45.0;

    /** 只做结构和长度校验，不证明票据真实性、服务器在线或玩家有权限。 */
    FGamePlatformResult Validate() const;
};

/**
 * FGamePlatformSessionSnapshot（平台会话公开快照）。
 *
 * 不包含密码、Authorization、Endpoint 或 TransferTicket。复制该值不会延长任何网络资源生命周期。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSESSION_API FGamePlatformSessionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    EGamePlatformSessionTransferState State = EGamePlatformSessionTransferState::Idle;

    /** 当前操作意图；Ready/Idle时保留最近一次意图供诊断，不作为权限判断。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    EGamePlatformSessionIntent Intent = EGamePlatformSessionIntent::Join;

    /** 当前恢复动作；ReconciliationRequired时禁止直接发起下一次连接。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    EGamePlatformSessionRecoveryState RecoveryState = EGamePlatformSessionRecoveryState::None;

    /** 是否必须先完成远端对账/清理。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    bool bRecoveryRequired = false;

    /** 是否允许直接发起新的会话操作；仅用于UI/流程决策，不替代BeginOperation内部复核。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    bool bCanRetry = false;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FGuid TransferOperationId;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FGamePlatformSessionConnectionBinding Binding;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    bool bAdmissionConfirmed = false;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FName ErrorCode = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Session")
    FString ErrorMessage;
};

/**
 * FGamePlatformSessionTransportCallbacks（会话传输回调集合）。
 *
 * Transport 可以从网络线程完成回调；Subsystem 会统一切回游戏线程并再次核对 TransferOperationId，
 * 因此 Transport 不应直接访问 ApplicationFlow、Widget 或其他项目 UObject。
 */
struct GAMEPLATFORMSESSION_API FGamePlatformSessionTransportCallbacks
{
    TFunction<void(FGamePlatformSessionConnectionBinding)> OnBindingPrepared;
    TFunction<void(FGamePlatformSessionConnectionBinding)> OnTravelCommitted;
    TFunction<void(EGamePlatformSessionTransferFact, FGamePlatformSessionConnectionBinding)> OnFact;
    TFunction<void(FName, FString)> OnFailed;
};

using FGamePlatformSessionDisconnectedCallback =
    TFunction<void(FGamePlatformSessionConnectionBinding)>;

/**
 * IGamePlatformSessionTransport（平台会话真实传输适配接口）。
 *
 * 实现方负责真实 ClientTravel/网络握手/服务器准入材料提交，不得返回固定成功结果。
 * 该接口不拥有 ApplicationFlow；它只把已验证事实回传给 Session。
 */
class GAMEPLATFORMSESSION_API IGamePlatformSessionTransport
{
public:
    virtual ~IGamePlatformSessionTransport() = default;

    virtual void BeginTransfer(
        const FGamePlatformSessionTransferRequest& Request,
        FGamePlatformSessionTransportCallbacks Callbacks) = 0;

    virtual void CancelTransfer(const FGuid& TransferOperationId) = 0;

    /**
     * Session四事实全部完成后通知Transport收敛本次操作监听，但保留当前连接的断线监控。
     * 默认空实现保持第三方Transport兼容。
     */
    virtual void CompleteTransfer(
        const FGuid& TransferOperationId,
        const FGamePlatformSessionConnectionBinding& Binding)
    {
    }

    /** 安装当前真实连接断线回调；实现不得在断线后重复回调旧Binding。 */
    virtual void SetDisconnectedCallback(
        FGamePlatformSessionDisconnectedCallback Callback)
    {
    }

    /** 离开当前已建立会话并清理本地网络；不得把凭据写入URL或日志。 */
    virtual void LeaveSession(const FGamePlatformSessionConnectionBinding& Binding) = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformSessionSnapshotChangedNative,
    const FGamePlatformSessionSnapshot&);

/**
 * UGamePlatformSessionClientSubsystem（平台会话客户端子系统）。
 *
 * 职责：
 * - 把公开 UE API 适配到私有纯 C++ 会话状态内核；
 * - 管理一次连接操作的身份、超时、取消和可信事实；
 * - 通过可注入 Transport 执行真实网络动作；
 * - 以事件方式发布只读快照，避免 UI 或 ApplicationFlow 逐帧轮询。
 *
 * 性能：
 * 仅在活动连接期间注册 0.1 秒低频 Ticker，用于单调时钟 Deadline（截止时间）；
 * 网络和业务动作完全事件驱动，不执行逐帧网络查询。
 */
UCLASS()
class GAMEPLATFORMSESSION_API UGamePlatformSessionClientSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UGamePlatformSessionClientSubsystem();
    UGamePlatformSessionClientSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformSessionClientSubsystem() override;
    virtual void Deinitialize() override;

    /** 安装真实传输适配器。切换适配器会取消旧活动操作，避免旧回调污染新连接。 */
    void SetTransport(TSharedPtr<IGamePlatformSessionTransport> InTransport);

    /**
     * 同步非敏感认证上下文。AccountId 为空表示退出登录。
     * AuthGeneration 来自 Online 子系统，只用于拒绝旧认证代次，不是访问令牌。
     */
    void SetAuthenticationContext(const FString& AccountId, const FGuid& AuthGeneration);

    /**
     * 显式开始 Join/Transfer/Reconnect 操作。相同OperationId的活动重入必须幂等，不能重复触发ClientTravel。
     */
    bool BeginOperation(
        EGamePlatformSessionIntent Intent,
        const FGamePlatformSessionTransferRequest& Request,
        FGamePlatformResult& OutResult);

    /**
     * 开始一次真实连接/跨服操作。成功仅表示操作被 Session 接纳并交给 Transport，
     * 不表示网络已连接、服务器已准入或世界已经可玩。
     */
    bool BeginTransfer(const FGamePlatformSessionTransferRequest& Request, FGamePlatformResult& OutResult);

    /** 显式断线重连入口；不会根据Current是否存在来猜测意图。 */
    bool Reconnect(const FGamePlatformSessionTransferRequest& Request, FGamePlatformResult& OutResult);

    /** 离开当前会话；只清理Session/网络，不退出Online账号。 */
    bool LeaveSession(FGamePlatformResult& OutResult);

    /** 报告当前可信Binding对应的真实网络断开；旧Binding不会清理新连接。 */
    bool NotifyDisconnected(
        const FGamePlatformSessionConnectionBinding& Binding,
        FGamePlatformResult& OutResult);

    /**
     * 远端查询完成后解除恢复屏障。ConfirmedBinding为空表示后端确认不存在远端绑定；
     * 非空时只能确认本地仍持有的同一来源绑定，不能由后端凭空制造Ready。
     */
    bool ResolveRemoteState(
        const FGuid& TransferOperationId,
        const FGamePlatformSessionConnectionBinding& ConfirmedBinding,
        FGamePlatformResult& OutResult);

    /** 取消当前活动操作；已越过不可回滚网络边界时可能进入 Uncertain（结果不确定）。 */
    bool CancelTransfer(FGamePlatformResult& OutResult);

    /**
     * 报告仅能由本地可信系统观察的事实。
     * NetworkConnected 与 AdmissionConfirmed 只能由 Transport 回报，此入口会拒绝伪造。
     */
    bool ReportLocalFact(
        const FGuid& TransferOperationId,
        EGamePlatformSessionTransferFact Fact,
        const FGamePlatformSessionConnectionBinding& Binding,
        FGamePlatformResult& OutResult);

    FGamePlatformSessionSnapshot GetSnapshot() const { return Snapshot; }

    FGamePlatformSessionSnapshotChangedNative& OnSessionChanged()
    {
        return SessionChanged;
    }

private:
    struct FRuntime;

    bool TickActiveOperation(float DeltaSeconds);
    void RefreshSnapshot(FName ErrorCode = NAME_None, FString ErrorMessage = FString());
    void HandleBindingPrepared(
        const FGuid& TransferOperationId,
        FGamePlatformSessionConnectionBinding Binding);
    void HandleTravelCommitted(
        const FGuid& TransferOperationId,
        FGamePlatformSessionConnectionBinding Binding);
    void HandleTransportFact(
        const FGuid& TransferOperationId,
        EGamePlatformSessionTransferFact Fact,
        FGamePlatformSessionConnectionBinding Binding);
    void HandleTransportFailure(
        const FGuid& TransferOperationId,
        FName ErrorCode,
        FString ErrorMessage);
    void StopTicker();

    TUniquePtr<FRuntime> Runtime;
    TSharedPtr<IGamePlatformSessionTransport> Transport;
    FGamePlatformSessionSnapshot Snapshot;
    FGamePlatformSessionSnapshotChangedNative SessionChanged;
};
