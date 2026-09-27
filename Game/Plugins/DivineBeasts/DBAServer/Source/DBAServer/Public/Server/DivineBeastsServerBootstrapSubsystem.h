#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Server/DivineBeastsServerRoleProfile.h"
#include "DivineBeastsServerBootstrapSubsystem.generated.h"

class UGamePlatformServerLifecycleSubsystem;

/** EDivineBeastsServerBootstrapState（神兽联盟服务器启动状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsServerBootstrapState : uint8
{
    Unconfigured,
    ProfileLoaded,
    WaitingForWorld,
    Registering,
    Registered,
    Ready,
    Draining,
    Stopped,
    Failed
};

/**
 * UDivineBeastsServerBootstrapSubsystem（神兽联盟服务器启动子系统）。
 * 仅专用服务器实例创建；读取角色Profile，不加载地图、不生成对象、不在Initialize联网。
 */
UCLASS()
class DBASERVER_API UDivineBeastsServerBootstrapSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 供服务器宿主按实测玩家数显式上报；过期状态或超过Profile容量时拒绝。 */
    bool ReportHeartbeat(int32 CurrentPlayers);
    /** 请求控制面停止新分配；不在这里销毁世界、终止比赛或踢出玩家。 */
    bool BeginDrain();
    /** 本地清理完成后结束本GameInstance的服务器生命周期状态。 */
    bool CompleteDrain();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Server")
    EDivineBeastsServerBootstrapState GetBootstrapState() const { return State; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Server")
    FName GetLastErrorCode() const { return LastErrorCode; }

    const FDivineBeastsServerRoleProfile* GetActiveProfile() const
    {
        return bHasProfile ? &ActiveProfile : nullptr;
    }

private:
    void LoadLaunchProfile();
    /** 观察属于当前GameInstance的新世界，并在其真正BeginPlay时进入服务器注册门禁。 */
    void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues InitializationValues);
    /** 切换当前观察世界；跨地图时先解除旧世界委托，避免旧回调污染新实例。 */
    void ObserveWorld(UWorld* World);
    /** 当前观察世界开始运行；从弱引用恢复世界后进入统一校验路径。 */
    void HandleObservedWorldBeginPlay();
    /** 解除当前世界BeginPlay委托并清空弱引用；可重复调用。 */
    void StopObservingWorld();
    void HandleWorldBeginPlay(UWorld* World);
    void RegisterValidatedWorld(UWorld& World);
    void HandleLifecycleChanged(const struct FGamePlatformServerLifecycleSnapshot& Snapshot);
    void SetFailed(FName ErrorCode);
    bool IsConfiguredWorldValid(UWorld& World, FString& OutReason) const;

    FDivineBeastsServerRoleProfile ActiveProfile;
    FName ActiveExperienceId = NAME_None;
    EDivineBeastsServerBootstrapState State = EDivineBeastsServerBootstrapState::Unconfigured;
    FName LastErrorCode = NAME_None;
    FDelegateHandle WorldInitializedHandle;
    FDelegateHandle WorldBeginPlayHandle;
    FDelegateHandle LifecycleChangedHandle;
    TWeakObjectPtr<UWorld> ObservedWorld;
    TWeakObjectPtr<UWorld> ValidatedWorld;
    bool bHasProfile = false;
    bool bWorldValidated = false;
};
