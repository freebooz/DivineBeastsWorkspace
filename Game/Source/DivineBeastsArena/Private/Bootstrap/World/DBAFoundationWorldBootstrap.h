#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformLoadingTypes.h"
#include "Types/GamePlatformRegion.h"
#include "DBAFoundationWorldExercisePolicy.h"
#include "DBAFoundationWorldBootstrap.generated.h"

class AActor;
class UWorld;
class IGamePlatformDataService;

/** 项目开发装配：只有显式-FoundationWorld才创建；不启动旧流程、不改默认地图或网络准入。 */
UCLASS(Transient)
class UDBAFoundationWorldBootstrap final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** Shipping、Commandlet及未显式授权的实例不创建；网络模式在真实World出现后再次检查。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    /** 只安装游戏线程低频轮询；服务获取延后到所有实例子系统初始化完成。 */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    /** 先停止新请求，再释放本操作、注册、Actor及Data租约；绝不取消他人的Loading。 */
    virtual void Deinitialize() override;
private:
    bool Tick(float DeltaSeconds);
    void BeginWorld(UWorld& World);
    void AdvanceWorld();
    void AcquireDefinition(const FPrimaryAssetId& Id, TSubclassOf<UGamePlatformDefinitionBase> Class);
    /** Busy时保留撤销句柄供下次轮询；只有真正释放才返回true。 */
    bool ReleaseOwnedResources();
    void Fail(FName Code);
    void Marker(const TCHAR* Event, const FGuid& Generation) const;
    /** Tick已安装后才发OpenLevel；返回true表示本轮正在Travel等待，不能初始化旧World。 */
    bool AdvanceExercise(UWorld* Current);
    /** 公开API驱动观察者，以实际延后回调推进，不以经过时间冒充事件完成。 */
    bool AdvanceRegionEvents();

    FTSTicker::FDelegateHandle Ticker;
    TWeakObjectPtr<UWorld> ActiveWorld;
    FPrimaryAssetId DefinitionId;
    FGuid RunId;
    FGuid AttemptId;
    FGuid WorldGeneration;
    FGuid PreviousGeneration;
    FGuid WorldOperation;
    DBAFoundationWorldExercise::FPolicy Exercise;
    TArray<FGamePlatformDataLease> Leases;
    TArray<FPrimaryAssetId> RegionDefinitions;
    TArray<FGamePlatformWorldRegistration> Providers;
    FGamePlatformWorldRegistration RegionSubscription;
    TArray<FGamePlatformId> RegionIdentities;
    TArray<TPair<FGamePlatformId,bool>> ExpectedRegionEvents;
    int32 ReceivedRegionEvents = 0;
    int32 ObserverStep = 0;
    FGamePlatformLoadingRegistration Factory;
    FGamePlatformLoadingHandle Operation;
    /** 本实例创建的Transient对象强引用；不保存到关卡，不复制到网络。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> ProviderActors;
    double DeadlineSeconds = 0;
    double ReadinessTimeoutSeconds = 60;
    FName DevelopmentServerRole;
    bool bStopping = false;
    bool bFailed = false;
    bool bInitialized = false;
    bool bRegionsReady = false;
    bool bReadyReported = false;
    bool bFirstReadyObserved = false;
    bool bReturnedToBootstrap = false;
    bool bPreviousResourcesReleased = false;
    bool bBusyReported = false;
    bool bOldGenerationRejected = false;
    bool bRegionEventsComplete = false;
};
