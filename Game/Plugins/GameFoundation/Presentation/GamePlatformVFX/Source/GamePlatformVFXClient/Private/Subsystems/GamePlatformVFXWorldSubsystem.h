#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Preloading/GamePlatformVFXPreloadCoordinator.h"
#include "TimerManager.h"
#include "GamePlatformVFXWorldSubsystem.generated.h"

class UGamePlatformVFXDefinition;
class UGamePlatformVFXCompositeDefinition;
class UNiagaraComponent;

/** 世界级 VFX 运行服务。该类位于 Private，调用方只依赖 IGamePlatformVFXService。 */
UCLASS()
class UGamePlatformVFXWorldSubsystem final : public UWorldSubsystem, public IGamePlatformVFXService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformVFXPlayResult Play(const FGamePlatformVFXRequest& Request) override;
    virtual bool Stop(const FGamePlatformVFXHandle& Handle, bool bImmediate = false) override;
    virtual bool UpdateParameters(const FGamePlatformVFXHandle& Handle, const FGamePlatformVFXParameters& Parameters) override;
    virtual EGamePlatformVFXLifecycleState GetState(const FGamePlatformVFXHandle& Handle) const override;
    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) override;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) override;
    virtual FGamePlatformVFXPreloadHandle Preload(const FPrimaryAssetId& DefinitionId) override;
    virtual bool ReleasePreload(const FGamePlatformVFXPreloadHandle& Handle) override;

    // CompositeRunner 专用内部协作入口。
    void AttachChildHandle(const FGamePlatformVFXHandle& Parent, const FGamePlatformVFXHandle& Child);
    void AttachCompositeTimer(const FGamePlatformVFXHandle& Parent, const FTimerHandle& TimerHandle);
    void CompleteComposite(const FGamePlatformVFXHandle& Parent);

private:
    int32 WorldGeneration = 0;
    FGamePlatformVFXCatalogRegistry CatalogRegistry;
    FGamePlatformVFXInstanceRegistry InstanceRegistry;
    FGamePlatformVFXPreloadCoordinator PreloadCoordinator;
    TMap<FGuid, TArray<FTimerHandle>> CompositeTimers;

    void BeginAsyncLoad(FGamePlatformVFXInstanceRecord& Record);
    void OnDefinitionLoaded(FGuid InstanceId);
    void OnNiagaraLoaded(FGuid InstanceId);
    void SpawnResolved(FGuid InstanceId, UGamePlatformVFXDefinition& Definition);
    void FailInstance(FGuid InstanceId, const FString& Reason);
    void ReleaseRecordResources(FGamePlatformVFXInstanceRecord& Record);
    int32 CountAmbientInstances() const;

    UFUNCTION()
    void HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent);
};
