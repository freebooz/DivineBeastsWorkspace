#pragma once

// 第三层Server/Editor公开启动合同：由Dedicated GameInstance承载，向宿主暴露只读状态与排空命令。
// Profile与世界/Boot操作身份归本实例；不拥有全局分配，不授予玩家准入，不负责载图或生成角色。
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Server/DivineBeastsServerRoleProfile.h"
#include "DivineBeastsServerBootstrapSubsystem.generated.h"

class UGamePlatformServerLifecycleSubsystem;
class FDivineBeastsWorldCharacterAdmission;

/** 神兽联盟本实例启动状态；游戏线程只读投影，枚举本身不是准入证明，不新增或复用已发布枚举序号。 */
UENUM(BlueprintType)
enum class EDivineBeastsServerBootstrapState : uint8
{
    Unconfigured,   // 默认：尚未读取合法启动Profile。
    ProfileLoaded,  // Profile已加载，承载世界与必要资源仍须验证。
    WaitingForWorld,// 等待本GameInstance的真实Dedicated世界BeginPlay。
    Registering,    // 控制面注册在途，尚不能接纳新玩家。
    Registered,     // 控制面已注册，真实世界/体验Ready门禁尚未发布完成。
    Ready,          // 当前实例已发布Ready；具体玩家出生仍须通过独立准入门禁。
    Draining,       // 停止新准入，允许当前活跃玩家按既定流程完成。
    Stopped,        // 控制面确认排空结束；不代表此枚举负责销毁世界。
    Failed          // 启动或承载失败；错误码给出原因，退休实例不能重新激活。
};

/**
 * UDivineBeastsServerBootstrapSubsystem（神兽联盟服务器启动子系统）。
 * 仅专用服务器实例创建，公开操作和状态读取限游戏线程；不加载地图、不生成对象、不在Initialize联网。
 * Profile/委托/Timer归本GameInstance生命周期，Deinitialize永久退休该对象；重新承载必须创建新实例。
 */
UCLASS()
class DBASERVER_API UDivineBeastsServerBootstrapSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** 按引擎创建钩子检查Outer；仅Dedicated且非Commandlet创建，不由调用者强行开启客户端实例。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    /** 引擎在游戏线程传入本实例子系统集合；读取Profile并观察世界，已永久退休时拒绝重新注册资源。 */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    /** 游戏线程先使原操作失效，再撤自有世界委托/Timer/准入与心跳；可重复清理，不允许对象再次承载。 */
    virtual void Deinitialize() override;

    /** 游戏线程供宿主按实测非负玩家数上报；仅Registered/Ready/Draining可用，平台拒绝过期状态或超容量时返回false。 */
    bool ReportHeartbeat(int32 CurrentPlayers);
    /** 游戏线程先停止本地新准入再请求控制面排空；不销毁世界或踢活跃玩家，非法状态/退出/服务拒绝返回false。 */
    bool BeginDrain();
    /** 游戏线程由宿主在本地清理完成后结束本实例排空；非Draining或平台服务拒绝返回false，不另销毁世界。 */
    bool CompleteDrain();

    /** 游戏线程读取当前本实例状态，默认Unconfigured；关闭后读取最后记录值，不能据此重新承载或授予Ready/玩家准入。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Server")
    EDivineBeastsServerBootstrapState GetBootstrapState() const { return State; }

    /** 游戏线程读取稳定诊断码，NAME_None表示当前无已记录错误；完整承载世界退休时保留要求新实例的原因，不清码或改变状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Server")
    FName GetLastErrorCode() const { return LastErrorCode; }

    /** 游戏线程借用本实例Profile的只读指针，尚未合法加载返回nullptr；对象存储由本实例拥有，不得跨关闭缓存或当作准入证明。 */
    const FDivineBeastsServerRoleProfile* GetActiveProfile() const
    {
        return bHasProfile ? &ActiveProfile : nullptr;
    }

private:
    friend class FDivineBeastsServerWorldRetirementTest;
    void LoadLaunchProfile();
    /** 观察属于当前GameInstance的新世界，并在其真正BeginPlay时进入服务器注册门禁。 */
    void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues InitializationValues);
    /** 已验证世界退出立即撤销准入并排空控制面；同一Boot不自动复用到下一张地图，须重新启动服务器实例。 */
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    /** 切换当前观察世界；跨地图时先解除旧世界委托，避免旧回调污染新实例。 */
    void ObserveWorld(UWorld* World);
    /** 当前观察世界开始运行；从弱引用恢复世界后进入统一校验路径。 */
    void HandleObservedWorldBeginPlay();
    /** 解除当前世界BeginPlay委托并清空弱引用；可重复调用。 */
    void StopObservingWorld();
    void HandleWorldBeginPlay(UWorld* World);
    void RegisterValidatedWorld(UWorld& World);
    void HandleLifecycleChanged(const struct FGamePlatformServerLifecycleSnapshot& Snapshot);
    /** 注册完成与真实Gameplay激活均可触发；未满足门禁时等待，不把地图BeginPlay当作可出生。 */
    void TryPublishReady();
    /** GT只核原实例/承载世界/Boot/不透明操作身份；不提供准入能力，外部服务返回后失效即停止原栈。 */
    bool IsBootstrapScopeCurrent(FGuid OperationId, TWeakObjectPtr<UWorld> World,
        TWeakObjectPtr<UGameInstance> Instance, const FString& BootId) const;
    void SetFailed(FName ErrorCode);
    bool IsConfiguredWorldValid(UWorld& World, FString& OutReason) const;
    /** 返回当前权威GameState中的连接玩家数；世界失效时返回-1使平台心跳Fail Closed。 */
    int32 GetCurrentPlayerCount() const;
    /** 本世界平台/Data真正就绪后启动一次体验；截止失败不继续发布可准入实例。 */
    void AdvanceGameplayBootstrap();
    FTimerHandle GameplayBootstrapTimer;
    double GameplayBootstrapDeadline=0;
    /** 当前已验证世界的项目角色准入适配；关闭/退休时先取消资料请求，防止旧响应初始化后继Pawn。 */
    TSharedPtr<FDivineBeastsWorldCharacterAdmission> WorldCharacterAdmission;

    FDivineBeastsServerRoleProfile ActiveProfile;
    FName ActiveExperienceId = NAME_None;
    EDivineBeastsServerBootstrapState State = EDivineBeastsServerBootstrapState::Unconfigured;
    FName LastErrorCode = NAME_None;
    /** 当前Dedicated Server进程唯一Boot身份；同一GameInstance生命周期固定，进程重启后变化。 */
    FString ServerBootId;
    FDelegateHandle WorldInitializedHandle;
    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle WorldBeginPlayHandle;
    FDelegateHandle LifecycleChangedHandle;
    TWeakObjectPtr<UWorld> ObservedWorld;
    TWeakObjectPtr<UWorld> ValidatedWorld;
    bool bHasProfile = false;
    bool bWorldValidated = false;
    /** 地图流送不会触发该标志；完整承载世界退出后保持终止栅栏，迟到Ready不能复活。 */
    bool bWorldRetired = false;
    /** 每次当前实例承载或Registered通知接管签发；失败/排空/关闭先失效，旧队列与同步外部返回不能尾写。 */
    FGuid BootstrapOperationId;
};
