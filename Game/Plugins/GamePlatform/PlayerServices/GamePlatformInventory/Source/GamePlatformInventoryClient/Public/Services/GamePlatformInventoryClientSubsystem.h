#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformInventoryTypes.h"
#include "GamePlatformInventoryClientSubsystem.generated.h"

class IGamePlatformInventoryClientTransport;

DECLARE_MULTICAST_DELEGATE(FGamePlatformInventoryClientChanged);

UCLASS()
class GAMEPLATFORMINVENTORYCLIENT_API UGamePlatformInventoryClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformInventoryClientTransport, ESPMode::ThreadSafe> InTransport);

    void ResetAccount();

    UFUNCTION(BlueprintPure, Category="Inventory")
    EGamePlatformInventoryClientState GetState() const { return State; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    int64 GetInventoryRevision() const { return Snapshot.InventoryRevision; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool HasPendingOperation() const { return Pending.Type != EGamePlatformInventoryOperationType::None; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    FGuid GetPendingOperationId() const { return Pending.OperationId; }

    const FGamePlatformInventorySnapshot& GetSnapshot() const { return Snapshot; }
    TArray<FGamePlatformInventoryItemInstance> GetSortedItems() const;
    TArray<FGamePlatformInventoryItemViewModel> GetViewModels() const;

    /** C++高频读取使用：按Snapshot代次缓存排序结果，避免每次UI刷新重新排序/分配。 */
    const TArray<FGamePlatformInventoryItemInstance>& GetSortedItemsView() const;

    /** C++高频读取使用：按Snapshot + Pending Operation缓存派生ViewModel。 */
    const TArray<FGamePlatformInventoryItemViewModel>& GetViewModelsView() const;
    const FGamePlatformInventoryItemInstance* FindItem(const FString& ItemInstanceId) const;

    bool RefreshSnapshot();
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
    uint64 SnapshotGeneration = 0;

    // Derived caches（派生缓存）只由Snapshot/Pending代次驱动，不成为第二份业务真源。
    mutable uint64 CachedSnapshotGeneration = ~uint64(0);
    mutable TArray<FGamePlatformInventoryItemInstance> CachedSortedItems;
    mutable TMap<FString, int32> CachedItemIndexByInstanceId;

    mutable uint64 CachedViewSnapshotGeneration = ~uint64(0);
    mutable FGuid CachedViewPendingOperationId;
    mutable EGamePlatformInventoryOperationType CachedViewPendingType =
        EGamePlatformInventoryOperationType::None;
    mutable TArray<FGamePlatformInventoryItemViewModel> CachedViewModels;

    TSharedPtr<IGamePlatformInventoryClientTransport, ESPMode::ThreadSafe> Transport;

    void EnsureSnapshotDerivedCache() const;
    void EnsureViewModelCache() const;

    bool BeginPendingOperation();
    bool SendPendingOperation();

    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
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

    bool ApplySnapshot(
        const FGamePlatformInventorySnapshot& NewSnapshot);

    void SetState(
        EGamePlatformInventoryClientState NewState,
        EGamePlatformInventoryError Error);
};
