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

private:
    friend class FGamePlatformArenaSafeDrainGateTest;
    /** 结果已由后端持久提交、比赛正式完成、实例身份匹配且无实际PlayerController时才允许回收。纯判定供原生回归复用，不代表后端或Agones已确认关闭。 */
    static bool CanRequestSafeDrain(bool bResultCommitted, bool bMatchCompleted,
        bool bSameGameServer, bool bHasLivePlayerControllers);
    /** 比赛结果真实提交后才启动本世界低频安全回收重试；从不踢活跃玩家。零人心跳发送后再请求控制面Drain，完成Drain才允许Agones Shutdown。 */
    void TryFinishArenaDrain();
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
    FTimerHandle SafeDrainTimer;
    /** 这些旗标只有后端确认结果后才启动；不将UI关窗或普通比赛Ending当成回收授权。 */
    bool bResultCommitted = false;
    bool bZeroHeartbeatRequested = false;
    bool bSafeDrainRequested = false;
    /** 结果后零人数回执必须严格晚于本次结果确认；防止旧0人心跳冒充可信安全迁出。 */
    uint64 ResultHeartbeatBaselineGeneration = 0;
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
