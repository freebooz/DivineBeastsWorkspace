// SSLM项目层Village狼散布与生命周期：复用World/Gameplay权限事实、导航和现有平台AI/Combat。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IGamePlatformGameplayService.h"
#include "TimerManager.h"
#include "DivineBeastsVillageWolfWorldSubsystem.generated.h"

class ADivineBeastsVillageWolf;
class ADivineBeastsVillageNPC;
class UGamePlatformExperienceComponent;
class UGamePlatformServerLifecycleSubsystem;
class UNavigationSystemV1;
class ANavigationData;
struct FGamePlatformCombatEvent;
struct FGamePlatformServerLifecycleSnapshot;

/** 狼群只在真实Village.Tutorial Dedicated Server生效；安全区禁止刷新、追击/咬击。
 * 玩法权威及怪物生命周期归本世界，而非PCG装饰或客户端表现。 */
UCLASS()
class UDivineBeastsVillageWolfWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void OnWorldEndPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    /** 由真实平台世界/体验双方签发的作用域；换地图/重启/排空前必须失效。 */
    struct FApprovedScope
    {
        FGuid WorldGeneration;
        int64 ExperienceEpoch = 0;
        bool operator==(const FApprovedScope& B) const
        { return WorldGeneration == B.WorldGeneration && ExperienceEpoch == B.ExperienceEpoch; }
    };

    /** 项目Config默认关闭，且必须显式确认客户端Fab外观已完成导入；拒绝不可见的攻击怪。 */
    bool HasVerifiedAnimalAssets() const;
    /** 从实际服务器角色、体验、地图和World服务获得本代次允许生成的许可。 */
    bool CaptureApprovedScope(FApprovedScope& OutScope) const;
    /** 订阅既有体验事实；避免每帧扫描世界/玩家。 */
    void ObserveExperience();
    /** 导航可能晚于服务器Ready完成；只订阅本世界通知，不轮询或在无落点时强刷。 */
    void ObserveNavigation();
    UFUNCTION() void HandleNavigationFinished(ANavigationData* NavigationData);
    /** 服务端Ready/Experience Active推动首次有限散布。 */
    void TryFillWolves();
    /** 本世界有界地表候选缓存；只读现有PCG排除和导航，不修改地图。 */
    bool BuildSpawnCandidates() const;
    /** 缓存候选+NavMesh+地形+碰撞+22米保护+锚点严格>24米，缺空间不强刷。 */
    bool FindSpawnTransform(int32 SpawnOrdinal, bool bVampire, FTransform& OutTransform) const;
    /** 在出生区外围导航可达且互不重叠的位置选取三名友好NPC的固定落点。 */
    bool FindNPCSpawnTransform(int32 SpawnOrdinal, FTransform& OutTransform) const;
    /** 每个真实Village实例仅维护三名服务器复制NPC；美术由客户端独立装配。 */
    void TryFillNPCs(const FApprovedScope& Scope);
    /** 世界拥有Actor/Controller及订阅；初始化失败销毁两者，不保留半活体。 */
    bool SpawnOneWolf(const FTransform& SpawnAt, bool bVampire);
    /** 只处理自己持有的已结算死亡，尸体延迟销毁并触发有界补充。 */
    void HandleWolfDeath(const FGamePlatformCombatEvent& Event);
    UFUNCTION() void HandleWolfDestroyed(AActor* DestroyedActor);
    /** 原世界/旧定时器不能补充后继世界的怪物。 */
    void ReplenishWolves(FGuid Ticket);
    void HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot& Snapshot);
    void HandleExperienceChanged(const FGamePlatformExperienceSnapshot& Snapshot);
    /** 撤销单世界所有权：Timer → 订阅 → Actor/Controller；不触碰玩家和地图。 */
    void CloseScope();

    UPROPERTY(Transient)
    TArray<TObjectPtr<ADivineBeastsVillageWolf>> OwnedWolves;
    UPROPERTY(Transient)
    TArray<TObjectPtr<ADivineBeastsVillageNPC>> OwnedNPCs;
    /** 当前世界的实际出生位置，不跟随狼追击/巡逻移动；避免死亡补刷在原锚点附近扎堆。 */
    TMap<TWeakObjectPtr<ADivineBeastsVillageWolf>, FVector> WolfSpawnAnchors;
    mutable TArray<FVector> CachedSpawnGrounds;
    mutable bool bSpawnCandidatesBuilt = false;
    TWeakObjectPtr<UGamePlatformExperienceComponent> ObservedExperience;
    TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> ObservedLifecycle;
    TWeakObjectPtr<UNavigationSystemV1> ObservedNavigation;
    FGamePlatformGameplayRegistration ExperienceSubscription;
    FDelegateHandle LifecycleSubscription;
    FTimerHandle ReplenishTimer;
    /** 初始生成分批计时器；死亡后进入14秒重生冷却时必须取消它。 */
    FTimerHandle SpawnBatchTimer;
    FGuid RespawnTicket;
    FGuid WorldOperation;
    FApprovedScope OwnedScope;
    /** 通用资产路径只有逻辑字符串，服务器从不加载狼Mesh/动画/贴图。 */
    FSoftObjectPath WolfAIAssetPath;
    /** 从服务端安全的WolfBlueprintClass软路径取得的生成类；绝不加载狼骨骼和材质。 */
    UPROPERTY(Transient)
    TObjectPtr<UClass> WolfActorClass = nullptr;
    bool bCreating = false;
    bool bClosing = false;
};
