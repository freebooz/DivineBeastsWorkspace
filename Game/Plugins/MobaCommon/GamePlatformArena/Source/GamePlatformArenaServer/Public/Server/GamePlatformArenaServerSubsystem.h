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
