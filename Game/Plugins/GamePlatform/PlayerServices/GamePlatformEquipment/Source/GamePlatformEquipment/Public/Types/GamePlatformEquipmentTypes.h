#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEquipmentError : uint8
{
    None,
    EquipmentNotLoaded,
    InvalidSlot,
    SlotNotSupported,
    ItemNotFound,
    ItemNotOwned,
    ItemNotEquippable,
    ItemAlreadyEquipped,
    ItemEquippedElsewhere,
    InventoryRevisionConflict,
    EquipmentRevisionConflict,
    OperationInProgress,
    DuplicateOperation,
    GameplayStateProhibitsEquip,
    AbilitySystemUnavailable,
    EquipmentDefinitionMissing,
    GameplayGrantFailed,
    PersistenceUnavailable,
    PersistenceOutcomeUnknown,
    RuntimeApplyFailed,
    SocketMissing,
    VisualLoadFailed,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipmentSlotState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FString ItemInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformPublicEquipmentSlotState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipmentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FString CharacterId;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 EquipmentRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    TArray<FGamePlatformEquipmentSlotState> Slots;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformPublicEquipmentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 PublicStateRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    TArray<FGamePlatformPublicEquipmentSlotState> Slots;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FGuid OperationId;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString CharacterId;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString ItemInstanceId;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedEquipmentRevision = 0;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedInventoryRevision = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformUnequipRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FGuid OperationId;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString CharacterId;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedEquipmentRevision = 0;
};
