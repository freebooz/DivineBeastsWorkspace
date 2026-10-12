// 每世界局部贴花组件/短期队列和GamePlatformData普通资源租约唯一所有者。
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Marks/IGamePlatformSurfaceMarkService.h"
#include "Types/GamePlatformDataLease.h"
#include "GamePlatformSurfaceMarkWorldSubsystem.generated.h"

class UDecalComponent;
class UGamePlatformSurfaceMarkCatalog;

/** 独立于现有全局雪、苔藓、湿润MPC，不创建Tick或权威碰撞。 */
UCLASS()
class UGamePlatformSurfaceMarkWorldSubsystem final
    : public UWorldSubsystem, public IGamePlatformSurfaceMarkService
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual FGamePlatformSurfaceMarkResult Submit(const FGamePlatformSurfaceMarkRequest& Request) override;
    virtual bool RefreshCatalog() override;    virtual FDelegateHandle AddSpawnedHandler(const FGamePlatformSurfaceMarkSpawned::FDelegate& Handler) override;
    virtual void RemoveSpawnedHandler(FDelegateHandle Handle) override;
    virtual void GetCounts(int32& OutActive, int32& OutPending) const override;

private:
    struct FActiveMark
    {
        TWeakObjectPtr<UDecalComponent> Component;
        double ExpireTimeSeconds = 0;
    };

    /** 不通过UPROPERTY复制UObject所有权；排队表只维持请求短期生命周期。 */
    UPROPERTY(Transient)
    TArray<FGamePlatformSurfaceMarkRequest> PendingRequests;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformSurfaceMarkCatalog> LoadedCatalog;

    FGamePlatformSurfaceMarkSpawned MarkSpawned;
    TArray<FActiveMark> ActiveMarks;
    TArray<FGuid> RecentEventIds;
    FGamePlatformDataLease CatalogLease;
    FGamePlatformDataLease MaterialsLease;
    int64 LoadGeneration = 0;
    bool bClosing = false;
    bool bLoadAttempted = false;
    bool bReady = false;

    class IGamePlatformDataService* GetDataService() const;
    bool BeginAssetLoad();
    void OnCatalogLoaded(int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void OnMaterialsLoaded(int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void ReleaseAssetLeases();
    void PruneActive();
    bool AlreadySeenEvent(const FGuid& EventId) const;
    void RememberEvent(const FGuid& EventId);
    EGamePlatformSurfaceMarkStatus SpawnLoadedMark(const FGamePlatformSurfaceMarkRequest& Request);
    FGamePlatformSurfaceMarkResult MakeResult(EGamePlatformSurfaceMarkStatus Status) const;
};
