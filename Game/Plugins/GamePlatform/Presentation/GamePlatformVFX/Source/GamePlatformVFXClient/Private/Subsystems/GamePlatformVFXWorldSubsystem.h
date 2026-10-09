// 本文件属于GamePlatform平台层 GamePlatformVFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
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
 * 一个Definition在同一World只持有一个元数据Data Lease，服务并发Play；Preload独立持有其定义和选中资源。
 */
struct FGamePlatformVFXCachedDefinitionEntry
{
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<UGamePlatformVFXDefinition> Definition;
    TMap<FGuid, FGamePlatformVFXPendingDefinitionRequest> PendingRequests;
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
    virtual FGamePlatformVFXPlaybackSnapshot GetPlaybackSnapshot(const FGamePlatformVFXHandle& Handle) const override;
    virtual FDelegateHandle AddCompletionHandler(const FGamePlatformVFXPlaybackCompleted::FDelegate& Handler) override;
    virtual void RemoveCompletionHandler(FDelegateHandle Handle) override;

    virtual FGamePlatformVFXPreloadHandle Preload(const FGamePlatformVFXRequest& Request) override;
    virtual bool CancelPreload(const FGamePlatformVFXPreloadHandle& Handle) override;
    virtual EGamePlatformVFXPreloadState GetPreloadState(const FGamePlatformVFXPreloadHandle& Handle, FString& OutError) const override;

    /** 旧低层工具兼容入口；标准 Gameplay 路径不得再次依赖 VFX Catalog 做语义解析。 */
    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) override;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) override;

private:
    friend class FGamePlatformVFXPredictionTerminalTest;
    friend class FGamePlatformVFXResourceOwnershipTest;
    friend class FGamePlatformVFXDataIntegrationTest;
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
    /** 定义元数据与本次选中资源分开租约，缺可选变体不会使基础定义不可读。 */
    EGamePlatformVFXDefinitionQueueResult QueueSelectedResources(UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request, const FGamePlatformVFXHandle& Handle);
    void HandleSelectedResourcesLoaded(FGamePlatformVFXHandle Handle,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    bool TryDefinitionFallback(const FGamePlatformVFXRequest& Request, const FGamePlatformVFXHandle& Handle);
    void ReleaseInstanceResources(const FGamePlatformVFXHandle& Handle);
    struct FInstanceResourceRequest
    {
        FGamePlatformVFXHandle Handle;
        FGamePlatformVFXRequest Request;
        FGamePlatformDataLease Lease;
        bool bFallbackMetadata = false;
        bool bLoading = false;
    };
    TMap<FGuid, FInstanceResourceRequest> InstanceResources;
    TMap<FGuid, TSet<FName>> FallbackVisitedDefinitions;
    /** 根Composite的总后代预算；每次出生累计，子实例结束不能归还总树预算。 */
    struct FCompositeRootBudget { FGamePlatformVFXHandle Handle; int32 MaxChildren = 0; int32 MaxDepth = 0; int32 CreatedChildren = 0; };
    TMap<FGuid, FCompositeRootBudget> CompositeRootBudgets;
    TMap<FGuid, FGuid> CompositeRootByInstance;
    TSet<FGuid> CleaningInstances;
    /** 独立预载事务；定义/资源租约在Ready后继续持有，取消不操作其他实例的需求。 */
    struct FPreloadRecord
    {
        FGamePlatformVFXPreloadHandle Handle;
        FGamePlatformVFXRequest Request;
        TMap<FName, FGamePlatformDataLease> Definitions;
        TArray<FGamePlatformDataLease> Resources;
        TSet<FGuid> CompletedLeases;
        int32 PendingLoads = 0;
        int32 MaxDepth = 8;
        int32 MaxDefinitions = 64;
        EGamePlatformVFXPreloadState State = EGamePlatformVFXPreloadState::Loading;
    };
    struct FPreloadTerminal { FGamePlatformVFXPreloadHandle Handle; EGamePlatformVFXPreloadState State; FString Error; };
    TMap<FGuid, FPreloadRecord> PreloadRecords;
    TMap<FGuid, FPreloadTerminal> PreloadTerminals;
    TArray<FGuid> PreloadTerminalOrder;
    int64 NextPreloadGeneration = 0;
    bool QueuePreloadDefinition(const FGamePlatformVFXPreloadHandle& Handle, FName DefinitionId, int32 Depth);
    void HandlePreloadDefinitionLoaded(FGamePlatformVFXPreloadHandle Handle, FName DefinitionId, int32 Depth,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void HandlePreloadResourcesLoaded(FGamePlatformVFXPreloadHandle Handle,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void FinishPreload(const FGamePlatformVFXPreloadHandle& Handle, EGamePlatformVFXPreloadState State, const FString& Error);
    bool EnsureDefinitionCacheCapacity();
    bool EvictOneCachedDefinition();
    void ReleaseAllCachedDefinitions();
    void RemoveDefinitionUse(const FGamePlatformVFXHandle& Handle);
    void TouchCachedDefinition(FName DefinitionId);

    /** Niagara动态多播完成回调；必须是UFUNCTION以便AddDynamic绑定。 */
    UFUNCTION()
    void HandleSystemFinished(UNiagaraComponent* Component);

    void CleanupInstance(const FGamePlatformVFXHandle& Handle, bool bStopComponent,
        EGamePlatformVFXPlaybackState State = EGamePlatformVFXPlaybackState::Completed,
        EGamePlatformVFXResultCode Code = EGamePlatformVFXResultCode::AlreadyCompleted,
        const FString& Diagnostic = FString());
    void CompletePlayback(const FGamePlatformVFXHandle& Handle, EGamePlatformVFXPlaybackState State,
        EGamePlatformVFXResultCode Code, const FString& Diagnostic);
    TMap<FGuid, FGamePlatformVFXPlaybackSnapshot> PlaybackSnapshots;
    TArray<FGuid> CompletedPlaybackOrder;
    FGamePlatformVFXPlaybackCompleted PlaybackCompleted;
    void ReleaseLease(const FGamePlatformDataLease& Lease) const;
    void ScheduleLifetime(const FGamePlatformVFXHandle& Handle, float Seconds);

    void AddDedupeHandle(const FGamePlatformVFXDedupeKey& Key, const FGamePlatformVFXHandle& Handle);
    void RemoveDedupeHandle(const FGamePlatformVFXHandle& Handle);

    struct FTerminalOccurrence { double ExpiresAtSeconds = 0.0; bool bCancelled = false; EGamePlatformVFXResultCode CompletionCode = EGamePlatformVFXResultCode::AlreadyCompleted; };
    void PruneTerminalOccurrences();
    void RecordTerminalOccurrence(const FGamePlatformVFXDedupeKey& Key, bool bCancelled);

    bool RegisterCompositeTimer(
        const FGamePlatformVFXHandle& ParentHandle,
        const FTimerHandle& TimerHandle);
    void ClearCompositeTimers(const FGamePlatformVFXHandle& ParentHandle);

    void UpdateRuntimeDiagnostics();

    bool ExecuteLoadedDefinition(
        UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ReservedHandle);

    bool PlayDefinitionId(
        FName DefinitionId,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle);

    FGamePlatformVFXCatalogRegistry CatalogRegistry;
    FGamePlatformVFXInstanceRegistry InstanceRegistry;

    /** DefinitionId -> World共享Data Lease与等待者。 */
    TMap<FName, FGamePlatformVFXCachedDefinitionEntry> DefinitionCache;
    /** 实例Handle -> DefinitionId，用于O(1)清理Pending/Active使用计数。 */
    TMap<FGuid, FName> DefinitionIdByHandle;

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
    /** 初始化/关闭世代；跨外部通知校验，防止旧服务关闭后继续受理纠正事务。 */
    uint64 WorldLifecycleGeneration = 1;

    /** 以注册句柄持有Catalog强引用；注销时可精确释放，避免Content Pack热切换泄漏。 */
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UGamePlatformVFXCatalog>> RegisteredCatalogObjects;
};
