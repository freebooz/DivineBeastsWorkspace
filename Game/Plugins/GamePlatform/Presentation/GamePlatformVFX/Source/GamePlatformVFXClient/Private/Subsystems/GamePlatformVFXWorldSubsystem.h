#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Types/GamePlatformDataLease.h"
#include "TimerManager.h"
#include "GamePlatformVFXWorldSubsystem.generated.h"

class UNiagaraComponent;
class UGamePlatformVFXDefinition;
struct FStreamableHandle;

/**
 * 世界级 VFX 服务实现。保持 Private，外部只能经 IGamePlatformVFXService 访问。
 * 所有公开调用仅允许游戏线程；Definition 统一通过 GamePlatformData World Lease 获取。
 */
UCLASS()
class UGamePlatformVFXWorldSubsystem final : public UWorldSubsystem, public IGamePlatformVFXService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformVFXResult Play(const FGamePlatformVFXRequest& Request) override;
    virtual bool Stop(const FGamePlatformVFXHandle& Handle) override;
    virtual bool IsActive(const FGamePlatformVFXHandle& Handle) const override;

    virtual FGamePlatformVFXPreloadHandle Preload(const FGamePlatformVFXRequest& Request) override;
    virtual bool CancelPreload(const FGamePlatformVFXPreloadHandle& Handle) override;

    /** 旧低层工具兼容入口；标准 Gameplay 路径不得再次依赖 VFX Catalog 做语义解析。 */
    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) override;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) override;

private:
    void HandleStartupCatalogsLoaded();
    FString MakeDedupeKey(const FGamePlatformVFXRequest& Request) const;
    FName ResolveDefinitionId(const FGamePlatformVFXRequest& Request, bool& bOutAmbiguous) const;
    bool QueueDefinitionLoad(
        FName DefinitionId,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ReservedHandle);
    void HandleDefinitionLoaded(
        FGamePlatformVFXHandle ReservedHandle,
        FGamePlatformVFXRequest Request,
        FGamePlatformDataLease Lease,
        const FGamePlatformResult& Result);
    void HandleSystemFinished(UNiagaraComponent* Component);
    void CleanupInstance(const FGamePlatformVFXHandle& Handle, bool bStopComponent);
    void ReleaseLease(const FGamePlatformDataLease& Lease) const;
    void ScheduleLifetime(const FGamePlatformVFXHandle& Handle, float Seconds);
    void PruneDedupeHandles();

    bool ExecuteLoadedDefinition(
        UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ReservedHandle);

    void PlayDefinitionId(
        FName DefinitionId,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle);

    FGamePlatformVFXCatalogRegistry CatalogRegistry;
    FGamePlatformVFXInstanceRegistry InstanceRegistry;
    TMap<FGuid, FGamePlatformDataLease> PendingDefinitionLeases;
    TMap<FGuid, FGamePlatformDataLease> ActiveDefinitionLeases;
    TMap<FGuid, FGamePlatformDataLease> ExplicitPreloadLeases;
    TMap<FGuid, FTimerHandle> LifetimeTimers;
    TMap<FString, FGamePlatformVFXHandle> DedupeHandles;
    TArray<FGamePlatformVFXRegistrationHandle> StartupCatalogHandles;
    TSharedPtr<FStreamableHandle> StartupCatalogLoadLease;
    bool bClosing = false;

    /** 以注册句柄持有Catalog强引用；注销时可精确释放，避免Content Pack热切换泄漏。 */
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UGamePlatformVFXCatalog>> RegisteredCatalogObjects;
};
