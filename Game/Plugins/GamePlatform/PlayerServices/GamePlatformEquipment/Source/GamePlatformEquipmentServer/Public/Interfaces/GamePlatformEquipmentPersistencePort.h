#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEquipmentTypes.h"

using FGamePlatformEquipmentLoadCompletion =
    TFunction<void(
        FGamePlatformEquipmentSnapshot,
        EGamePlatformEquipmentError)>;

using FGamePlatformEquipmentMutationCompletion =
    TFunction<void(
        FGamePlatformEquipmentSnapshot,
        EGamePlatformEquipmentError)>;

class GAMEPLATFORMEQUIPMENTSERVER_API IGamePlatformEquipmentPersistencePort
{
public:
    virtual ~IGamePlatformEquipmentPersistencePort() = default;

    virtual bool BeginLoadEquipment(
        const FString& PlayerId,
        const FString& CharacterId,
        FGamePlatformEquipmentLoadCompletion Completion) = 0;

    virtual bool BeginEquip(
        const FString& PlayerId,
        const FGamePlatformEquipRequest& Request,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;

    virtual bool BeginUnequip(
        const FString& PlayerId,
        const FGamePlatformUnequipRequest& Request,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;

    virtual bool BeginQueryOperationResult(
        const FString& PlayerId,
        const FString& CharacterId,
        const FGuid& OperationId,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;
};
