#pragma once

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
    FString GameServerId;
    FString ServerBootId;
    FString WorldId;
    FString ExperienceId;
    FString ProtocolVersion;
    uint64 ServerStartGeneration = 0;

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

    FGuid OperationId;
    FString ReservationId;
    FString AttemptId;
    TArray<uint8> Credential;

    bool IsValid() const;
    void ResetSensitive();
};

/**
 * FGamePlatformServerVerifiedAdmission（服务端验证后的非敏感准入投影）。
 * 只有可信Provider完成领取/提交并返回权威Epoch后才能创建；该结构本身不是新的认证凭据。
 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerVerifiedAdmission
{
    FGuid AdmissionId;
    FGuid ConnectionId;
    FString PlayerId;
    FString SessionId;
    /** GameSessionId（游戏会话绑定身份）由控制面签票时生成；非凭据，用于客户端/服务器一致性核对。 */
    FString GameSessionId;
    FString AssignmentId;
    FString ReservationId;
    FString ServerInstanceId;
    FString ServerBootId;
    FString WorldId;
    FString ExperienceId;
    FString ProtocolVersion;
    uint64 ConnectionGeneration = 0;
    uint64 SessionEpoch = 0;
    FDateTime AuthorityUntil;

    bool IsStructurallyValid() const;
    bool MatchesTarget(const FGamePlatformServerAdmissionTarget& Target) const;
};

/** FGamePlatformServerAdmissionResult（准入验证结果）；错误码必须脱敏。 */
struct GAMEPLATFORMSERVER_API FGamePlatformServerAdmissionResult
{
    bool bSucceeded = false;
    FName ErrorCode = NAME_None;
    FGamePlatformServerVerifiedAdmission Admission;
};

using FGamePlatformServerAdmissionCompletion =
    TFunction<void(FGamePlatformServerAdmissionResult)>;
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

    static FName GetModularFeatureName();

    virtual void ValidateAdmission(
        const FGamePlatformServerAdmissionTarget& Target,
        FGuid ConnectionId,
        uint64 ConnectionGeneration,
        FGamePlatformServerAdmissionProof Proof,
        FGamePlatformServerAdmissionCompletion Completion) = 0;

    virtual void ReleaseAdmission(
        const FGamePlatformServerVerifiedAdmission& Admission,
        FGamePlatformServerAdmissionCompletion Completion) = 0;

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

    FGamePlatformServerAdmissionChangedNative& OnAdmissionChanged()
    {
        return AdmissionChanged;
    }

private:
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

    FGamePlatformServerAdmissionTarget ActiveTarget;
    TMap<TWeakObjectPtr<APlayerController>, FGamePlatformServerVerifiedAdmission> VerifiedAdmissions;
    TMap<FGuid, FPendingAdmission> PendingAdmissions;
    FGamePlatformServerAdmissionChangedNative AdmissionChanged;
    uint64 NextConnectionGeneration = 0;
    uint64 OperationGeneration = 0;
};
