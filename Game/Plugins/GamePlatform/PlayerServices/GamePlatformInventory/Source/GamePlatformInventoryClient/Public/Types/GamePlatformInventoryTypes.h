#pragma once

#include "CoreMinimal.h"
#include "GamePlatformInventoryTypes.generated.h"

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

    bool IsValid() const
    {
        return !ItemInstanceId.IsEmpty() &&
               !ItemDefinitionId.IsNone() &&
               Quantity > 0 &&
               !ContainerId.IsNone() &&
               SlotIndex >= 0 &&
               Revision > 0;
    }
};

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

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventorySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 InventoryRevision = 0;

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
