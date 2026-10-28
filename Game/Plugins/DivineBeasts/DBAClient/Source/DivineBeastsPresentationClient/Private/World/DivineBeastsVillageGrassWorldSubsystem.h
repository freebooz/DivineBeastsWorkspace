// 新手村 Landscape Grass 客户端资源租约与名称覆盖，不改变权威地形或服务器内容。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsVillageGrassWorldSubsystem.generated.h"

class ALandscapeProxy;
class ULandscapeGrassType;
class ULandscapeComponent;
class ULevel;
class ACharacter;
class UMaterialParameterCollection;
class UDivineBeastsVillageGrassWorldSubsystem;

/** 将一个角色的原生移动通知投影到 Fab 交互输入，不查询或改变玩法权威。 */
UCLASS(Transient)
class UDivineBeastsVillageGrassContactComponent final : public UActorComponent
{
    GENERATED_BODY()
public:
    void Bind(UDivineBeastsVillageGrassWorldSubsystem* OwnerSubsystem, int32 Slot);
    void SetSlot(int32 Slot) { SlotIndex = Slot; }
    int32 GetSlot() const { return SlotIndex; }
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UFUNCTION() void OnMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
    TWeakObjectPtr<UDivineBeastsVillageGrassWorldSubsystem> Subsystem;
    int32 SlotIndex = INDEX_NONE;
};

UCLASS()
class UDivineBeastsVillageGrassWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    friend class UDivineBeastsVillageGrassContactComponent;
    void TryRequestFor(ALandscapeProxy* Landscape);
    void OnReady(int64 Generation, const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result);
    void OnActorSpawned(AActor* Actor);
    void OnLevelAdded(ULevel* Level, UWorld* World);
    void Bind(ALandscapeProxy* Landscape);
    void UnbindAll();
    void AttachCharacter(ACharacter* Character);
    void UpdateContact(int32 Slot, ACharacter* Character);
    void RemoveContact(int32 Slot, const UDivineBeastsVillageGrassContactComponent* Component);
    void ClearContact(int32 Slot);

    struct FPreviousOverride
    {
        TWeakObjectPtr<ALandscapeProxy> Landscape;
        int32 GrassIndex = INDEX_NONE;
        int32 FlowersIndex = INDEX_NONE;
        bool bHadGrass = false;
        bool bHadFlowers = false;
        struct FComponentTypes
        {
            TWeakObjectPtr<ULandscapeComponent> Component;
            int32 GrassIndex = INDEX_NONE;
            int32 FlowersIndex = INDEX_NONE;
        };
        TArray<FComponentTypes> Components;
    };
    TArray<FPreviousOverride> Bound;
    /** 保持原草种至本世界解除覆盖，避免回退前被GC回收。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULandscapeGrassType>> RetainedOriginalTypes;
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<ULandscapeGrassType> GrassType;
    TWeakObjectPtr<ULandscapeGrassType> FlowerType;
    TWeakObjectPtr<UMaterialParameterCollection> ContactCollection;
    TWeakObjectPtr<UDivineBeastsVillageGrassContactComponent> Contacts[4];
    TArray<TWeakObjectPtr<UDivineBeastsVillageGrassContactComponent>> Observers;
    FDelegateHandle ActorHandle;
    FDelegateHandle LevelHandle;
    int64 RequestGeneration = 0;
    bool bActive = false;
    bool bRequestAttempted = false;
};
