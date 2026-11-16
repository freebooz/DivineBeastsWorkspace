#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlatformArenaServerSubsystem.generated.h"

class FGamePlatformArenaServerBackendClient;
class FGamePlatformArenaServerCoordinator;
class APlayerController;
class UGamePlatformServerAdmissionSubsystem;
struct FGamePlatformServerVerifiedAdmission;
class UGamePlatformServerLifecycleSubsystem;
struct FGamePlatformServerLifecycleSnapshot;
class AGamePlatformArenaGameMode;
enum class EGamePlatformArenaStartAbortReason : uint8;

/** UGamePlatformArenaServerSubsystem（竞技服务器世界子系统）。
 *  Dedicated Server进入MainArena世界后自动读取安全环境配置并拉取Assignment。
 */
UCLASS()
class GAMEPLATFORMARENASERVER_API UGamePlatformArenaServerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void OnWorldEndPlay(UWorld& InWorld) override;

private:
    friend class FGamePlatformArenaSafeDrainGateTest;
    /** 后端已持久确认终态、权威阶段一致、实例身份匹配且无实际PlayerController时才允许回收。 */
    static bool CanRequestSafeDrain(bool bTerminalConfirmed, bool bTerminalPhase,
        bool bSameGameServer, bool bHasLivePlayerControllers);
    /** 后端确认结果或开局终止后启动低频安全回收。等待真实离场及新零人心跳回执。 */
    void TryFinishArenaDrain();
    void BeginConfirmedDrain(AGamePlatformArenaGameMode& Mode);
    void ReportMatchStartAborted(EGamePlatformArenaStartAbortReason Reason);
    void SubmitStartAbortAttempt();
    /** 游戏线程把当前连接的已验证非敏感事实桥接到可信Roster；所有身份来自控制面，客户端不能指定PlayerId。 */
    void TryAdmitVerifiedConnection(APlayerController& Controller);
    void RefreshVerifiedConnections();
    void HandleAdmissionChanged(const APlayerController* Controller,
        const FGamePlatformServerVerifiedAdmission& Admission, bool bAdmitted);
    /** 到期先移除本连接绑定再失活；旧AdmissionId不能撤销重连的新绑定。 */
    void ExpireVerifiedConnection(TWeakObjectPtr<APlayerController> Controller, FGuid AdmissionId);
    struct FAdmittedConnection
    {
        FGuid AdmissionId;
        FString PlayerId;
        FTimerHandle AuthorityDeadlineTimer;
    };
    TWeakObjectPtr<UGamePlatformServerAdmissionSubsystem> AdmissionSubsystem;
    TMap<TWeakObjectPtr<APlayerController>, FAdmittedConnection> AdmittedConnections;
    FDelegateHandle AdmissionChangedHandle;
    /** 当前世界的结果通知订阅与生命周期定时器均由本WorldSubsystem持有；世界退出必解绑。 */
    TWeakObjectPtr<AGamePlatformArenaGameMode> ObservedArenaMode;
    FDelegateHandle ResultPendingHandle;
    FDelegateHandle StartAbortedHandle;
    FTimerHandle StartAbortRetryTimer;
    TOptional<EGamePlatformArenaStartAbortReason> StartAbortReason;
    float NextStartAbortRetrySeconds = 1.0f;
    FTimerHandle SafeDrainTimer;
    bool bTerminalConfirmed = false;
    bool bZeroHeartbeatRequested = false;
    bool bSafeDrainRequested = false;
    /** 只处理终态确认之后的零人数回执。 */
    uint64 TerminalHeartbeatBaselineGeneration = 0;
    TSharedPtr<FGamePlatformArenaServerBackendClient> BackendClient;
    TSharedPtr<FGamePlatformArenaServerCoordinator> Coordinator;
    /** 当前世界分配订阅先等待真实注册；失败只做有界传输恢复，不以UI或Tick读业务。 */
    void TryLoadAssignment();
    void HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot& Snapshot);
    TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> LifecycleSubsystem;
    FDelegateHandle LifecycleChangedHandle;
    FTimerHandle AssignmentRetryTimer;
    uint64 AssignmentGeneration=0;
    int32 AssignmentFailures=0;
    bool bWorldStarted=false;
    bool bAssignmentRequestPending=false;
    bool bAssignmentLoaded=false;
    bool bAssignmentFailed=false;

};
