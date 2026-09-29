#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformInventoryTypes.h"
#include "GamePlatformInventoryClientSubsystem.generated.h"

class FSubsystemCollectionBase;
class IGamePlatformInventoryClientTransport;
class UGamePlatformOnlineClientSubsystem;
struct FGamePlatformAuthSnapshot;

DECLARE_MULTICAST_DELEGATE(FGamePlatformInventoryClientChanged);

/**
 * UGamePlatformInventoryClientSubsystem（背包客户端子系统）维护当前 LocalPlayer（本地玩家）
 * 的只读背包快照和最多一个未决写操作。长期真源始终位于 PlayerDataService（玩家数据服务）。
 *
 * 默认运行时自动跟随 GamePlatformOnlineClient（平台在线客户端）的认证状态；
 * ConfigureAuthenticatedAccount（配置认证账号）保留给自动化测试和受控自定义 Transport。
 */
UCLASS()
class GAMEPLATFORMINVENTORYCLIENT_API UGamePlatformInventoryClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 测试/受控适配入口；正式游戏由 Online 认证事件自动配置。 */
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformInventoryClientTransport, ESPMode::ThreadSafe> InTransport);

    /** 取消当前账号在途请求并清空所有派生缓存；不会注销 Online 账号。 */
    void ResetAccount();

    UFUNCTION(BlueprintPure, Category="Inventory")
    EGamePlatformInventoryClientState GetState() const { return State; }

    /** 最近一次状态迁移对应的稳定错误；Ready 状态也可保留最近一次可恢复业务错误。 */
    UFUNCTION(BlueprintPure, Category="Inventory")
    EGamePlatformInventoryError GetLastError() const { return LastError; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    int64 GetInventoryRevision() const { return Snapshot.InventoryRevision; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool HasPendingOperation() const
    {
        return Pending.Type != EGamePlatformInventoryOperationType::None;
    }

    UFUNCTION(BlueprintPure, Category="Inventory")
    FGuid GetPendingOperationId() const { return Pending.OperationId; }

    const FGamePlatformInventorySnapshot& GetSnapshot() const { return Snapshot; }
    TArray<FGamePlatformInventoryItemInstance> GetSortedItems() const;
    TArray<FGamePlatformInventoryItemViewModel> GetViewModels() const;

    /** C++高频读取使用：按 Snapshot 代次缓存排序结果，避免每次 UI 刷新重新排序/分配。 */
    const TArray<FGamePlatformInventoryItemInstance>& GetSortedItemsView() const;

    /** C++高频读取使用：按 Snapshot + Pending Operation 缓存派生 ViewModel。 */
    const TArray<FGamePlatformInventoryItemViewModel>& GetViewModelsView() const;
    const FGamePlatformInventoryItemInstance* FindItem(const FString& ItemInstanceId) const;

    /**
     * 请求完整快照。存在未决写操作时，只有 RevisionConflict（修订冲突）触发的内部对账
     * 可以刷新，避免普通刷新误清理结果未知的 OperationId。
     */
    bool RefreshSnapshot();

    /**
     * 结果未知后的显式恢复：先查询原 OperationId；仅后端明确 OperationNotFound 时，
     * 才复用同一个 OperationId 重发一次。
     */
    bool RetryPendingOperation();

    FGuid RequestMove(
        const FString& ItemInstanceId,
        FName TargetContainerId,
        int32 TargetSlotIndex);

    FGuid RequestSplit(
        const FString& SourceItemInstanceId,
        int32 SplitQuantity,
        FName TargetContainerId,
        int32 TargetSlotIndex);

    FGuid RequestMerge(
        const FString& SourceItemInstanceId,
        const FString& TargetItemInstanceId);

    FGuid RequestSetQuickbar(
        int32 SlotIndex,
        const FString& ItemInstanceId);

    FGuid RequestClearQuickbar(int32 SlotIndex);

    /** 背包状态、快照、Pending 或错误变化通知；高频 UI 不应 Tick 轮询。 */
    FGamePlatformInventoryClientChanged OnChanged;

private:
    struct FPendingOperation
    {
        EGamePlatformInventoryOperationType Type =
            EGamePlatformInventoryOperationType::None;
        FGuid OperationId;
        FGamePlatformInventoryMoveRequest Move;
        FGamePlatformInventorySplitRequest Split;
        FGamePlatformInventoryMergeRequest Merge;
        FGamePlatformInventoryQuickbarRequest Quickbar;

        void Reset()
        {
            *this = FPendingOperation();
        }
    };

    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;
    EGamePlatformInventoryClientState State =
        EGamePlatformInventoryClientState::Uninitialized;
    EGamePlatformInventoryError LastError =
        EGamePlatformInventoryError::None;

    FGamePlatformInventorySnapshot Snapshot;
    FPendingOperation Pending;

    /** SnapshotGeneration（快照代次）只在成功应用新快照后增加，用于派生缓存失效。 */
    uint64 SnapshotGeneration = 0;

    /** SnapshotRequestGeneration（快照请求代次）隔离同账号迟到响应。 */
    uint64 SnapshotRequestGeneration = 0;
    bool bSnapshotRequestInFlight = false;

    /**
     * 只有 RevisionConflict 置位后，带 Pending 的全量快照才允许清理旧操作；
     * 普通刷新与 Operation 查询恢复不能使用该权限。
     */
    bool bConflictSnapshotReconcile = false;

    // Derived caches（派生缓存）只由 Snapshot/Pending 代次驱动，不成为第二份业务真源。
    mutable uint64 CachedSnapshotGeneration = ~uint64(0);
    mutable TArray<FGamePlatformInventoryItemInstance> CachedSortedItems;
    mutable TMap<FString, int32> CachedItemIndexByInstanceId;

    mutable uint64 CachedViewSnapshotGeneration = ~uint64(0);
    mutable FGuid CachedViewPendingOperationId;
    mutable EGamePlatformInventoryOperationType CachedViewPendingType =
        EGamePlatformInventoryOperationType::None;
    mutable TArray<FGamePlatformInventoryItemViewModel> CachedViewModels;

    TSharedPtr<IGamePlatformInventoryClientTransport, ESPMode::ThreadSafe> Transport;

    /** OnlineSubsystem（在线子系统）只提供认证状态和安全请求通道；背包不读取/保存 Token。 */
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> OnlineSubsystem;
    FDelegateHandle AuthStateChangedHandle;

    void BindOnlineAuthentication();
    void UnbindOnlineAuthentication();
    void HandleAuthStateChanged(const FGamePlatformAuthSnapshot& AuthSnapshot);

    void EnsureSnapshotDerivedCache() const;
    void EnsureViewModelCache() const;

    bool BeginPendingOperation();
    bool SendPendingOperation();

    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
        uint64 ExpectedSnapshotRequestGeneration,
        bool bExpectedConflictReconcile,
        FGamePlatformInventorySnapshot NewSnapshot,
        EGamePlatformInventoryError Error);

    void HandleOperationQueryCompleted(
        uint64 ExpectedGeneration,
        FGuid ExpectedOperationId,
        FGamePlatformInventoryMutationResult Result,
        EGamePlatformInventoryError Error);

    void HandleMutationCompleted(
        uint64 ExpectedGeneration,
        FGuid ExpectedOperationId,
        FGamePlatformInventoryMutationResult Result,
        EGamePlatformInventoryError Error);

    bool ApplySnapshot(FGamePlatformInventorySnapshot NewSnapshot);

    static bool IsOutcomeUnknownError(EGamePlatformInventoryError Error);

    void SetState(
        EGamePlatformInventoryClientState NewState,
        EGamePlatformInventoryError Error);
};
