#pragma once

// 平台层Dedicated Server实例准入契约：安全握手调用，控制面拥有身份与授权；本地账本只持非敏感投影。
// 子系统所有接口/原生通知在游戏线程；Provider可异步完成，投影发布后仍需当前Target/代次/有效期核验。

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformServerAdmissionSubsystem.generated.h"

class APlayerController;

/**
 * FGamePlatformServerAdmissionTarget（服务器准入目标身份）。
 * 仅来自部署/服务器控制面，不接受客户端覆盖；BootId与ProtocolVersion用于拒绝旧进程和协议错配。
 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerAdmissionTarget
{
    /** 控制面实例身份；非空、最多1024字符且不得含换行。 */
    FString GameServerId;
    /** 本次服务器启动身份；进程重启必须改变，不复用旧票据授权。 */
    FString ServerBootId;
    /** 当前承载世界身份；与准入响应精确相等，不能用地图显示名代替。 */
    FString WorldId;
    /** 已验证体验身份；教学/训练属于体验，不新增服务器角色。 */
    FString ExperienceId;
    /** 锁定协议版本字符串；不兼容明确拒绝准入。 */
    FString ProtocolVersion;
    /** 本实例启动代次，必须大于0；由可信组合根提供。 */
    uint64 ServerStartGeneration = 0;

    /** 只检查本地结构与文本边界，不能证明控制面授权。 */
    bool IsValid() const;
};

/**
 * FGamePlatformServerAdmissionProof（一次性准入证明）。
 * 该类型仅供C++安全握手适配层传入，不是USTRUCT、不能进入蓝图/反射；析构时主动清零原始证明字节。
 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerAdmissionProof
{
    FGamePlatformServerAdmissionProof() = default;
    ~FGamePlatformServerAdmissionProof();
    FGamePlatformServerAdmissionProof(const FGamePlatformServerAdmissionProof&) = delete;
    FGamePlatformServerAdmissionProof& operator=(const FGamePlatformServerAdmissionProof&) = delete;
    FGamePlatformServerAdmissionProof(FGamePlatformServerAdmissionProof&& Other) noexcept;
    FGamePlatformServerAdmissionProof& operator=(FGamePlatformServerAdmissionProof&& Other) noexcept;

    /** 一次握手操作身份，必须有效；取消与完成以该身份及私有代次关联。 */
    FGuid OperationId;
    /** 已预留目标身份与本次尝试身份，非空且各最多1024字符，不作为独立认证凭据。 */
    FString ReservationId;
    FString AttemptId;
    /** 原始敏感证明32至16384字节，只移动、禁止复制/日志/持久化，释放时清零。 */
    TArray<uint8> Credential;

    /** 检查操作、文本和字节长度；真实领取与授权必须交给Provider。 */
    bool IsValid() const;
    /** 幂等清零并释放证明，不改变已验证投影。 */
    void ResetSensitive();
};

/**
 * FGamePlatformServerVerifiedAdmission（服务端验证后的非敏感准入投影）。
 * 只有可信Provider完成领取/提交并返回权威Epoch后才能创建；该结构本身不是新的认证凭据。
 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerVerifiedAdmission
{
    /** Provider签发的授权记录与真实连接身份；有效GUID，不能由客户端自报。 */
    FGuid AdmissionId;
    FGuid ConnectionId;
    /** 已验证玩家及认证会话；非空、有界，不含密码/Token。 */
    FString PlayerId;
    FString SessionId;
    /** GameSessionId（游戏会话绑定身份）由控制面签票时生成；非凭据，用于客户端/服务器一致性核对。 */
    FString GameSessionId;
    /** 当前服务器分配身份；不能推算MatchId或角色身份。 */
    FString AssignmentId;
    /** 控制面已验签的目标比赛身份；普通世界迁移为空，不能由AssignmentId或客户端推算。 */
    FString MatchId;
    /** 控制面预留身份；普通文本结构上限同Target。 */
    FString ReservationId;
    /** 必须精确匹配当前Target的实例、启动、世界、体验和协议身份。 */
    FString ServerInstanceId;
    FString ServerBootId;
    FString WorldId;
    FString ExperienceId;
    FString ProtocolVersion;
    /** 真实连接正代次与控制面会话正Epoch；旧连接/旧授权不得复用。 */
    uint64 ConnectionGeneration = 0;
    uint64 SessionEpoch = 0;
    /** UTC授权截止时刻；小于或等于当前UTC即不可继续查询为有效。 */
    FDateTime AuthorityUntil;

    /** 结构与当前有效期检查，不签发授权；普通世界MatchId允许空。 */
    bool IsStructurallyValid() const;
    /** 结构有效且五项目标身份完全一致才返回true，不接受客户端目标覆盖。 */
    bool MatchesTarget(const FGamePlatformServerAdmissionTarget& Target) const;
};

/** FGamePlatformServerAdmissionResult（准入验证结果）；错误码必须脱敏。 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerAdmissionResult
{
    /** 默认失败；成功才允许消费Admission，失败码不得包含原始证明。 */
    bool bSucceeded = false;
    FName ErrorCode = NAME_None;
    /** 成功的非敏感投影；失败时调用方必须忽略。 */
    FGamePlatformServerVerifiedAdmission Admission;
};

/** 终态按值交付，Provider至多一次；允许同步重入，调用方须重验作用域。 */
using FGamePlatformServerAdmissionCompletion =
    TFunction<void(FGamePlatformServerAdmissionResult)>;
/** 游戏线程发布当前Controller投影/撤销；bool为本地有效性，普通委托不会自动跨网络。 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FGamePlatformServerAdmissionChangedNative,
    const APlayerController*,
    const FGamePlatformServerVerifiedAdmission&,
    bool);

/**
 * IGamePlatformServerAdmissionProvider（服务器准入提供者）。
 *
 * 实现方必须把Proof与真实ConnectionId、目标实例BootId和控制面授权绑定；不得仅信任客户端PlayerId、IP或UniqueId。
 * Provider可以使用HTTP/gRPC/本地可信代理，但不得记录Credential原文，完成回调至多一次。
 */
class GAMEPLATFORMSERVER_API IGamePlatformServerAdmissionProvider
    : public IModularFeature
{
public:
    virtual ~IGamePlatformServerAdmissionProvider() = default;

    /** 唯一机制注册键；进程注册只存机制，不存用户/世界授权。 */
    static FName GetModularFeatureName();

    /** 接收移动敏感Proof并验证目标/连接/代次；同步或异步终态一次，失败必须真实返回脱敏码。 */
    virtual void ValidateAdmission(
        const FGamePlatformServerAdmissionTarget& Target,
        FGuid ConnectionId,
        uint64 ConnectionGeneration,
        FGamePlatformServerAdmissionProof Proof,
        FGamePlatformServerAdmissionCompletion Completion) = 0;

    /** 释放该投影拥有的控制面绑定，不能撤销后继连接；Completion可为空，禁止留下本地半有效状态。 */
    virtual void ReleaseAdmission(
        const FGamePlatformServerVerifiedAdmission& Admission,
        FGamePlatformServerAdmissionCompletion Completion) = 0;

    /** 取消本次握手并释放Provider自有资源；旧代次迟到回调由调用方拒绝，不得变成新授权。 */
    virtual void CancelOperation(const FGuid& OperationId) = 0;
};

/**
 * UGamePlatformServerAdmissionSubsystem（游戏平台服务器准入子系统）。
 *
 * 按Dedicated Server GameInstance隔离，只接受C++安全握手层提交一次性Proof；平台层不自行从URL/Options提取票据。
 * 验证通过后仅保存非敏感投影，并通过查询接口供第三层组合根桥接到GamePlatformGameplay准入Sink。
 */
UCLASS()
class GAMEPLATFORMSERVER_API UGamePlatformServerAdmissionSubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    /** 由服务器组合根在当前实例身份完成校验后设置；活动准入存在时禁止换目标。 */
    bool ConfigureTarget(const FGamePlatformServerAdmissionTarget& Target);

    /** 游戏线程撤销当前目标：先拒绝新准入，再取消在途操作、发布撤销并释放本作用域已验证连接。 */
    void ResetTarget(FName Reason = TEXT("ServerAdmissionTargetClosed"));
    /** 游戏线程停止新准入并取消未发布握手；保留现有Verified连接与Target供玩家自然排空。
     * 取消通知内允许ResetTarget/Deinitialize升级为完整撤销；世界退出必须使用完整撤销。
     */
    void StopAcceptingAdmissions(FName Reason = TEXT("ServerAdmissionDraining"));

    /**
     * 对实际PlayerController连接发起准入验证；同一Controller同一时刻最多一个操作。
     * Proof按值移动给Provider，子系统不长期保存原始证明。
     */
    bool BeginAdmission(
        APlayerController& Controller,
        FGamePlatformServerAdmissionProof Proof,
        FGamePlatformServerAdmissionCompletion Completion);

    /** 精确释放当前Controller的已验证绑定；旧回调和旧Controller不能撤销新连接。 */
    bool ReleaseAdmission(
        APlayerController& Controller,
        FGamePlatformServerAdmissionCompletion Completion);

    /** 只读查询当前连接已验证投影；不返回原始票据。 */
    bool GetVerifiedAdmission(
        const APlayerController& Controller,
        FGamePlatformServerVerifiedAdmission& OutAdmission) const;

    /** 借用本实例原生通知；订阅者拥有解绑责任，可同步关停，事件不提供跨网络复制。 */
    FGamePlatformServerAdmissionChangedNative& OnAdmissionChanged()
    {
        return AdmissionChanged;
    }

private:
    friend class FGamePlatformServerAdmissionResetTest;
    struct FPendingAdmission
    {
        FGuid OperationId;
        FGuid ConnectionId;
        uint64 Generation = 0;
        uint64 ConnectionGeneration = 0;
        TWeakObjectPtr<APlayerController> Controller;
        FGamePlatformServerAdmissionCompletion Completion;
    };

    IGamePlatformServerAdmissionProvider* ResolveUniqueProvider() const;
    void CompleteAdmission(
        FGuid OperationId,
        uint64 Generation,
        FGamePlatformServerAdmissionResult Result);
    static FGamePlatformServerAdmissionResult MakeFailure(FName ErrorCode);
    /** 调用前已持有清理栅栏；先取走账本，再执行外部取消/失败通知，避免重入修改迭代器。 */
    void CancelPendingAdmissions(FName Reason);

    FGamePlatformServerAdmissionTarget ActiveTarget;
    TMap<TWeakObjectPtr<APlayerController>, FGamePlatformServerVerifiedAdmission> VerifiedAdmissions;
    TMap<FGuid, FPendingAdmission> PendingAdmissions;
    FGamePlatformServerAdmissionChangedNative AdmissionChanged;
    uint64 NextConnectionGeneration = 0;
    uint64 OperationGeneration = 0;
    /** 外部取消/完成委托允许重入；清理期间禁止重新配置目标或接受请求。 */
    bool bResettingTarget = false;
    /** 正常排空的独立栅栏；完整Reset可在停止取消回调中重入，禁止重新打开准入。 */
    bool bStoppingAdmissions = false;
    /** Deinitialize后永久拒绝重配/准入；ResetTarget自身可用于仍存活实例的显式世界撤销。 */
    bool bIsClosing = false;
    /** ConfigureTarget后才允许新握手；排空保持目标可查询，但此标志不可由普通握手恢复。 */
    bool bAcceptingAdmissions = false;
};
