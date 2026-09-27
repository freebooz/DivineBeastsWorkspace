#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/GamePlatformEquipmentGASGrantPort.h"
#include "Types/GamePlatformEquipmentTypes.h"
#include "GamePlatformEquipmentServerComponent.generated.h"

class IGamePlatformEquipmentPersistencePort;
class UAbilitySystemComponent;
class UGamePlatformEquipmentComponent;
class UGamePlatformEquipmentDefinition;

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENTSERVER_API UGamePlatformEquipmentServerComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentServerComponent();

    bool InitializeEquipmentRuntime(
        const FString& InPlayerId,
        const FString& InCharacterId,
        UGamePlatformEquipmentComponent* InStateComponent,
        TSharedPtr<IGamePlatformEquipmentPersistencePort, ESPMode::ThreadSafe> InPersistence,
        TSharedPtr<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe> InGrantPort);

    void RegisterDefinition(
        UGamePlatformEquipmentDefinition* Definition);

    bool BindAvatar(
        UAbilitySystemComponent* InAbilitySystem,
        int32 InAvatarGeneration);

    EGamePlatformEquipmentError RequestEquip(
        const FGamePlatformEquipRequest& Request);

    EGamePlatformEquipmentError RequestUnequip(
        const FGamePlatformUnequipRequest& Request);

    bool Reconcile();

    UFUNCTION(BlueprintPure, Category="Equipment")
    bool IsEquipmentGameplayReady() const
    {
        return bReady && !bRuntimeError;
    }

    UFUNCTION(BlueprintPure, Category="Equipment")
    EGamePlatformEquipmentError GetLastError() const
    {
        return LastError;
    }

private:
    FString PlayerId;
    FString CharacterId;

    TWeakObjectPtr<UGamePlatformEquipmentComponent> StateComponent;
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

    TSharedPtr<IGamePlatformEquipmentPersistencePort, ESPMode::ThreadSafe>
        Persistence;
    TSharedPtr<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe>
        GrantPort;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformEquipmentDefinition>>
        Definitions;

    FGamePlatformEquipmentSnapshot Snapshot;
    TMap<FName, FGamePlatformEquipmentGameplayGrantHandle> SlotGrantHandles;

    int32 AvatarGeneration = 0;
    int32 EquipmentRuntimeGeneration = 0;
    bool bReady = false;
    bool bRuntimeError = false;
    bool bPersistenceInFlight = false;
    EGamePlatformEquipmentError LastError = EGamePlatformEquipmentError::None;

    void HandleLoadCompleted(
        FGamePlatformEquipmentSnapshot Loaded,
        EGamePlatformEquipmentError Error);

    void HandleMutationCompleted(
        FGuid OperationId,
        FGamePlatformEquipmentSnapshot Persisted,
        EGamePlatformEquipmentError Error);

    bool ApplyRuntimeSnapshot(
        const FGamePlatformEquipmentSnapshot& Persisted);

    void RevokeAllRuntimeGrants();
    void PublishSnapshot();
};
