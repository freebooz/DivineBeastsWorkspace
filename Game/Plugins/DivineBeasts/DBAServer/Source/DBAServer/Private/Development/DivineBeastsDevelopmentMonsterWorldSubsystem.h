// 项目服务器开发训练装配：只拥有本World的一只怪物、出生查询、死亡订阅及单次10秒重生Timer。
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IGamePlatformGameplayService.h"
#include "TimerManager.h"
#include "DivineBeastsDevelopmentMonsterWorldSubsystem.generated.h"

class ADivineBeastsDevelopmentMonster;
class UGamePlatformExperienceComponent;
class UGamePlatformServerLifecycleSubsystem;
struct FGamePlatformCombatEvent;
struct FGamePlatformServerLifecycleSnapshot;

/** 仅Server/Editor实际运行的权威Village Tutorial世界装配，不接管玩家准入或客户端资源。 */
UCLASS()
class UDivineBeastsDevelopmentMonsterWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    /** 引擎在Actor EndPlay前调用；先撤自有Timer/Actor和监听，迟到队列无法重建。 */
    virtual void OnWorldEndPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    friend class FDivineBeastsDevelopmentMonsterWorldGateTest;
    friend class FDivineBeastsDevelopmentMonsterWorldExitTest;
    friend class FDivineBeastsDevelopmentMonsterSpawnSafetyTest;

    /** 世界/体验运行事实，只借用已验证身份；不能由客户端提交或替代真实服务检查。 */
    struct FApprovedWorldScope
    {
        /** 平台World事实的GUID，只有与Active体验一致才有效；旧世界不能复用。 */
        FGuid WorldContextGeneration;
        /** 平台体验运行代次；同世界体验撤销/切换也使原装配退休。 */
        int64 ExperienceEpoch = 0;
        bool operator==(const FApprovedWorldScope& Other) const
        { return WorldContextGeneration == Other.WorldContextGeneration && ExperienceEpoch == Other.ExperienceEpoch; }
    };

    /** 明确核Shared正式Village角色和Tutorial体验，Training/Main不作为此开发怪物入口。 */
    static bool IsVillageTutorialIdentity(FName ServerRoleId, const FGamePlatformId& ExperienceId);
    /** GT重新采样实际Profile、控制面Ready、World Ready和Experience Active；失败不授予生成资格。 */
    bool CaptureApprovedWorldScope(FApprovedWorldScope& OutScope) const;
    /** 弱订阅实际AuthGameMode体验组件，登记可同步重入，因此返回句柄须复核操作身份。 */
    void ObserveExperience();
    /** 门禁与安全落点完整成功才创建唯一自有Actor；初始化失败撤本装配，不伪造成功。 */
    void TryCreateOwnedMonster();
    /** 控制面Ready推动生成；Drain/Stopped/Failed单向关闭且不重新占有新作用域。 */
    void HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot& Snapshot);
    /** Active推动生成；体验退出、失败及已拥有作用域的代次切换撤Actor/Timer。 */
    void HandleExperienceChanged(const FGamePlatformExperienceSnapshot& Snapshot);
    /** 只读实际PlayerStart，检查地面法线及完整胶囊占位；没有安全候选明确失败，不返回固定坐标。 */
    bool FindSafeSpawnTransform(FTransform& OutTransform, const AActor* IgnoredActor = nullptr) const;
    /** 借用Combat已提交死亡值；重复本代次事件不重置原10秒截止，后继死亡撤旧自有Timer。 */
    void HandleOwnedMonsterDeath(const FGamePlatformCombatEvent& Event);
    /** 10秒自有回调，先撤本Ticket再复核世界/目标代次及安全位置；失败保持死亡，不清后继Timer。 */
    void RespawnOwnedMonster(FGuid Ticket, int32 ExpectedGeneration);
    /** 自有Actor被外部销毁也退休装配，不能由迟到死亡通知重新生成。 */
    UFUNCTION() void HandleOwnedMonsterDestroyed(AActor* DestroyedActor);
    /** 永久关闭原装配作用域，先失效再释放；只销毁自有Actor，绝不保存地图或PlayerStart。 */
    void CloseOwnedScope();

    /** 唯一被本装配创建/销毁的Actor；不是玩家或外部Actor的登记表。 */
    UPROPERTY(Transient) TObjectPtr<ADivineBeastsDevelopmentMonster> OwnedMonster;
    /** 借用世界体验及GameInstance生命周期，句柄只撤本调用者订阅。 */
    TWeakObjectPtr<UGamePlatformExperienceComponent> ObservedExperience;
    TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> ObservedLifecycle;
    FGamePlatformGameplayRegistration ExperienceSubscription;
    FDelegateHandle LifecycleSubscription;
    FDelegateHandle DeathSubscription;
    /** 单次重生Timer，单位秒的固定开发策略为10；退出及Ticket摘除只清此句柄。 */
    FTimerHandle RespawnTimer;
    /** World操作身份和当前重生身份，均在退出先失效。 */
    FGuid WorldOperationId;
    FGuid RespawnOperationId;
    /** 成功拥有Actor时捕获的实际世界/体验事实，不接受客户端覆盖。 */
    FApprovedWorldScope OwnedWorldScope;
    /** 已安排重生的正数死亡代次；0无待重生，用于同代次死亡事件幂等。 */
    int32 PendingRespawnGeneration = 0;
    /** 单向退出栅栏及Spawn同步重入保护，均不通过Tick轮询。 */
    bool bClosing = false;
    bool bCreatingMonster = false;
};
