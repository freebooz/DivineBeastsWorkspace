#pragma once

#include "CoreMinimal.h"
#include "GamePlatformInventoryTypes.generated.h"

/** EGamePlatformInventoryClientState（背包客户端状态）描述本地只读缓存与单个写事务的生命周期。 */
UENUM(BlueprintType)
enum class EGamePlatformInventoryClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Mutating,
    Reconciling,
    Error
};

/** EGamePlatformInventoryError（背包稳定错误）只由机器错误码映射；禁止解析服务端自然语言消息。 */
UENUM(BlueprintType)
enum class EGamePlatformInventoryError : uint8
{
    None,
    InventoryNotLoaded,
    ItemNotFound,
    DefinitionNotFound,
    InvalidQuantity,
    StackNotSupported,
    StackLimitExceeded,
    SlotOutOfRange,
    SlotOccupied,
    ContainerNotFound,
    RevisionConflict,
    DuplicateOperation,
    OperationInProgress,
    OperationNotFound,
    InventoryFull,
    ConsumeNotAllowed,
    InsufficientQuantity,
    OutcomeUnknown,
    BackendUnavailable,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
};

UENUM()
enum class EGamePlatformInventoryOperationType : uint8
{
    None,
    Move,
    Split,
    Merge,
    SetQuickbar,
    ClearQuickbar
};

/** FGamePlatformInventoryContainerSnapshot（容器快照）描述服务端权威容量边界。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryContainerSnapshot
{
    GENERATED_BODY()

    /** ContainerId（容器编号）是平台中立稳定标识，例如 main；不是项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = NAME_None;

    /** Capacity（容量）表示当前已解锁槽位数，合法槽位范围为 [0, Capacity)。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Capacity = 0;

    bool IsValid() const
    {
        return !ContainerId.IsNone() && Capacity > 0;
    }
};

/** FGamePlatformInventoryItemInstance（物品实例）是服务端权威长期持有记录的客户端投影。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = TEXT("main");

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName InstanceState = TEXT("active");

    /** MaxStackSize（权威堆叠上限）来自服务端快照；客户端 Definition 中的同名字段仅用于显示，不可作为规则真源。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    bool IsValid() const
    {
        return !ItemInstanceId.IsEmpty() &&
               !ItemDefinitionId.IsNone() &&
               Quantity > 0 &&
               !ContainerId.IsNone() &&
               SlotIndex >= 0 &&
               Revision > 0 &&
               MaxStackSize > 0 &&
               Quantity <= MaxStackSize;
    }
};

/** FGamePlatformInventoryQuickbarSlot（快捷栏槽位）只引用已有物品实例，不拥有物品。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryQuickbarSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 Revision = 0;
};

/** FGamePlatformInventorySnapshot（背包快照）是客户端唯一长期背包缓存真源。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventorySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 InventoryRevision = 0;

    /** Containers（容器）用于校验槽位容量；项目层不得自行复制第二套容量真源。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryContainerSnapshot> Containers;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryItemInstance> Items;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryQuickbarSlot> Quickbar;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMutationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGuid OperationId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGamePlatformInventorySnapshot Snapshot;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MovedQuantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 RemainingQuantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    bool bDuplicate = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemDefinitionView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DisplayNameKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DescriptionKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName Category = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName IconId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FName> ClientTags;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = NAME_None;

    /** MaxStackSize（权威堆叠上限）用于 UI 判断可合并数量；实际写操作仍由服务端再次验证。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    bool bPending = false;

    /** UI/Content层可用此稳定Id解析显示定义；这里不硬引用Widget或资产类。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DisplayDefinitionHandle = NAME_None;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMoveRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString ItemInstanceId;
    FName TargetContainerId = TEXT("main");
    int32 TargetSlotIndex = INDEX_NONE;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventorySplitRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString SourceItemInstanceId;
    int32 SplitQuantity = 0;
    FName TargetContainerId = TEXT("main");
    int32 TargetSlotIndex = INDEX_NONE;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMergeRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString SourceItemInstanceId;
    FString TargetItemInstanceId;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryQuickbarRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    int32 SlotIndex = INDEX_NONE;
    FString ItemInstanceId;
};
