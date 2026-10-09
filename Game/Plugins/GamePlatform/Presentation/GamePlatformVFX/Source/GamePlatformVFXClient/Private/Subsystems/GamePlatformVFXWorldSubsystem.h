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

enum class EGamePlatformVFXDefinitionQueueResult : uint8
{
    Failed,
    Queued,
    Executed
};

/** 去重键类型；使用结构化值避免高频Play路径构造FString。 */
enum class EGamePlatformVFXDedupeKind : uint8
{
    None,
    Request,
    Activation
};

/** 预测/确认/纠正共享的结构化去重键。 */
struct FGamePlatformVFXDedupeKey
{
    EGamePlatformVFXDedupeKind Kind = EGamePlatformVFXDedupeKind::None;
    FGuid Id;
    int64 PredictionKey = 0;

    bool IsValid() const
    {
        return Kind != EGamePlatformVFXDedupeKind::None && Id.IsValid();
    }

    friend bool operator==(const FGamePlatformVFXDedupeKey& A, const FGamePlatformVFXDedupeKey& B)
    {
        return A.Kind == B.Kind && A.Id == B.Id && A.PredictionKey == B.PredictionKey;
    }

    friend uint32 GetTypeHash(const FGamePlatformVFXDedupeKey& Key)
    {
        uint32 Hash = HashCombine(GetTypeHash(static_cast<uint8>(Key.Kind)), GetTypeHash(Key.Id));
        return HashCombine(Hash, GetTypeHash(Key.PredictionKey));
    }
};

/** 同一个共享Definition加载完成前等待执行的单个VFX实例。 */
struct FGamePlatformVFXPendingDefinitionRequest
{
    FGamePlatformVFXHandle Handle;
    FGamePlatformVFXRequest Request;
};

/**
 * World级共享Definition缓存项。
 * 一个Definition在同一World只持有一个GamePlatformData Lease，并服务并发Play与Preload。
 */
struct FGamePlatformVFXCachedDefinitionEntry
{
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<UGamePlatformVFXDefinition> Definition;
    TMap<FGuid, FGamePlatformVFXPendingDefinitionRequest> PendingRequests;
    TSet<FGuid> PreloadHandles;
    int32 ActiveUsers = 0;
    uint64 LastUsedSerial = 0;
    bool bLoading = false;
};

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
    friend class FGamePlatformVFXPredictionTerminalTest;
    void HandleStartupCatalogsLoaded();
    FGamePlatformVFXDedupeKey MakeDedupeKey(const FGamePlatformVFXRequest& Request) const;
    FName ResolveDefinitionId(const FGamePlatformVFXRequest& Request, bool& bOutAmbiguous) const;

    EGamePlatformVFXDefinitionQueueResult QueueDefinitionLoad(
        FName DefinitionId,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ReservedHandle);
    void HandleCachedDefinitionLoaded(
        FName DefinitionId,
        FGamePlatformDataLease Lease,
        const FGamePlatformResult& Result);
    bool EnsureDefinitionCacheCapacity();
    bool EvictOneCachedDefinition();
    void ReleaseAllCachedDefinitions();
    void RemoveDefinitionUse(const FGamePlatformVFXHandle& Handle);
    void TouchCachedDefinition(FName DefinitionId);

    /** Niagara动态多播完成回调；必须是UFUNCTION以便AddDynamic绑定。 */
    UFUNCTION()
    void HandleSystemFinished(UNiagaraComponent* Component);

    void CleanupInstance(const FGamePlatformVFXHandle& Handle, bool bStopComponent);
    void ReleaseLease(const FGamePlatformDataLease& Lease) const;
    void ScheduleLifetime(const FGamePlatformVFXHandle& Handle, float Seconds);

    void AddDedupeHandle(const FGamePlatformVFXDedupeKey& Key, const FGamePlatformVFXHandle& Handle);
    void RemoveDedupeHandle(const FGamePlatformVFXHandle& Handle);

    struct FTerminalOccurrence { double ExpiresAtSeconds = 0.0; bool bCancelled = false; };
    void PruneTerminalOccurrences();
    void RecordTerminalOccurrence(const FGamePlatformVFXDedupeKey& Key, bool bCancelled);

    void RegisterCompositeTimer(
        const FGamePlatformVFXHandle& ParentHandle,
        const FTimerHandle& TimerHandle);
    void ClearCompositeTimers(const FGamePlatformVFXHandle& ParentHandle);

    void UpdateRuntimeDiagnostics();

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

    /** DefinitionId -> World共享Data Lease与等待者。 */
    TMap<FName, FGamePlatformVFXCachedDefinitionEntry> DefinitionCache;
    /** 实例Handle -> DefinitionId，用于O(1)清理Pending/Active使用计数。 */
    TMap<FGuid, FName> DefinitionIdByHandle;
    /** PreloadHandle -> DefinitionId。 */
    TMap<FGuid, FName> PreloadDefinitionIds;

    TMap<FGuid, FTimerHandle> LifetimeTimers;
    TMap<FGuid, TArray<FTimerHandle>> CompositeStepTimers;

    TMap<FGamePlatformVFXDedupeKey, FGamePlatformVFXHandle> DedupeHandles;
    TMap<FGuid, FGamePlatformVFXDedupeKey> DedupeKeysByHandle;

    /** 完成/取消历史最多MaxDedupeEntries项、保留30秒；压力淘汰最早到期记录，旧世界不可见。 */
    TMap<FGamePlatformVFXDedupeKey, FTerminalOccurrence> TerminalOccurrences;

    TArray<FGamePlatformVFXRegistrationHandle> StartupCatalogHandles;
    TSharedPtr<FStreamableHandle> StartupCatalogLoadLease;

    int32 PendingInstanceCount = 0;
    int32 PeakTrackedInstances = 0;
    uint64 DefinitionCacheSerial = 0;
    bool bClosing = false;

    /** 以注册句柄持有Catalog强引用；注销时可精确释放，避免Content Pack热切换泄漏。 */
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UGamePlatformVFXCatalog>> RegisteredCatalogObjects;
};
