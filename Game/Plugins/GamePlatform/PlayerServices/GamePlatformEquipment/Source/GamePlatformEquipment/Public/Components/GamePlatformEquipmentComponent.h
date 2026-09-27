#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformEquipmentTypes.h"
#include "GamePlatformEquipmentComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FGamePlatformEquipmentChanged);

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENT_API UGamePlatformEquipmentComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    const FGamePlatformEquipmentSnapshot& GetOwnerSnapshot() const
    {
        return OwnerSnapshot;
    }

    const FGamePlatformPublicEquipmentSnapshot& GetPublicSnapshot() const
    {
        return PublicSnapshot;
    }

    UFUNCTION(BlueprintPure, Category="Equipment")
    int32 GetEquipmentRuntimeGeneration() const
    {
        return EquipmentRuntimeGeneration;
    }

    void SetServerSnapshots(
        const FGamePlatformEquipmentSnapshot& InOwnerSnapshot,
        int32 InRuntimeGeneration);

    FGamePlatformEquipmentChanged OnOwnerEquipmentChanged;
    FGamePlatformEquipmentChanged OnPublicEquipmentChanged;

private:
    UPROPERTY(ReplicatedUsing=OnRep_OwnerSnapshot)
    FGamePlatformEquipmentSnapshot OwnerSnapshot;

    UPROPERTY(ReplicatedUsing=OnRep_PublicSnapshot)
    FGamePlatformPublicEquipmentSnapshot PublicSnapshot;

    UPROPERTY(Replicated)
    int32 EquipmentRuntimeGeneration = 0;

    UFUNCTION()
    void OnRep_OwnerSnapshot();

    UFUNCTION()
    void OnRep_PublicSnapshot();

    static FGamePlatformPublicEquipmentSnapshot BuildPublicSnapshot(
        const FGamePlatformEquipmentSnapshot& Source);
};
