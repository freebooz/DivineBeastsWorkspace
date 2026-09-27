#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Preloading/GamePlatformVFXPreloadCoordinator.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "GamePlatformVFXWorldSubsystem.generated.h"

class UGamePlatformVFXDefinition;
struct FStreamableHandle;

/** 世界级 VFX 服务实现。保持 Private，外部只能经 IGamePlatformVFXService 访问。 */
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

    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) override;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) override;

private:
    void HandleStartupCatalogsLoaded();
    FString MakeDedupeKey(const FGamePlatformVFXRequest& Request) const;
    void PruneDedupeHandles();
    void PruneInstanceLeases();

    bool ExecuteLoadedDefinition(
        UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ReservedHandle);

    void PlayDefinitionSoft(
        const TSoftObjectPtr<UGamePlatformVFXDefinition>& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle);

    FGamePlatformVFXCatalogRegistry CatalogRegistry;
    FGamePlatformVFXPreloadCoordinator PreloadCoordinator;
    FGamePlatformVFXInstanceRegistry InstanceRegistry;
    TMap<FGuid, FGamePlatformVFXPreloadHandle> PendingInstancePreloads;
    TMap<FString, FGamePlatformVFXHandle> DedupeHandles;
    TArray<FGamePlatformVFXRegistrationHandle> StartupCatalogHandles;
    TSharedPtr<FStreamableHandle> StartupCatalogLoadLease;

    /** 以注册句柄持有Catalog强引用；注销时可精确释放，避免Content Pack热切换泄漏。 */
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UGamePlatformVFXCatalog>> RegisteredCatalogObjects;
};
