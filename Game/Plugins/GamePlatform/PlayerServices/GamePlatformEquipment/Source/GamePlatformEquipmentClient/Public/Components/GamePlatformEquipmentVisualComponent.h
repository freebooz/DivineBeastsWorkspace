#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GamePlatformEquipmentVisualComponent.generated.h"

class UGamePlatformEquipmentComponent;
class UGamePlatformEquipmentVisualDefinition;
struct FStreamableHandle;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENTCLIENT_API UGamePlatformEquipmentVisualComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentVisualComponent();

    void RegisterVisualDefinition(
        UGamePlatformEquipmentVisualDefinition* Definition);

    void BindEquipmentComponent(
        UGamePlatformEquipmentComponent* InEquipmentComponent);

    void BindAvatar(
        USkeletalMeshComponent* InAvatarMesh,
        int32 InAvatarGeneration);

    void RefreshVisuals();

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<UGamePlatformEquipmentComponent> EquipmentComponent;
    TWeakObjectPtr<USkeletalMeshComponent> AvatarMesh;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformEquipmentVisualDefinition>>
        Definitions;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UStaticMeshComponent>>
        ActiveStaticMeshes;

    TMap<FName, TSharedPtr<FStreamableHandle>>
        ActiveLoadHandles;

    int32 AvatarGeneration = 0;
    uint64 VisualRequestGeneration = 0;

    void ClearVisuals();

    void RequestStaticMesh(
        FName SlotId,
        UGamePlatformEquipmentVisualDefinition* Definition,
        uint64 ExpectedRequestGeneration,
        int32 ExpectedAvatarGeneration);
};
