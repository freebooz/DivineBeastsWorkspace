// 新手村 Landscape Grass 客户端资源租约与名称覆盖，不改变权威地形或服务器内容。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsVillageGrassWorldSubsystem.generated.h"

class ALandscapeProxy;
class ULandscapeGrassType;
class ULevel;

UCLASS()
class UDivineBeastsVillageGrassWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    void TryRequestFor(ALandscapeProxy* Landscape);
    void OnReady(int64 Generation, const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result);
    void OnActorSpawned(AActor* Actor);
    void OnLevelAdded(ULevel* Level, UWorld* World);
    void Bind(ALandscapeProxy* Landscape);
    void UnbindAll();

    struct FPreviousOverride
    {
        TWeakObjectPtr<ALandscapeProxy> Landscape;
        TWeakObjectPtr<ULandscapeGrassType> Grass;
        TWeakObjectPtr<ULandscapeGrassType> Flowers;
        bool bHadGrass = false;
        bool bHadFlowers = false;
    };
    TArray<FPreviousOverride> Bound;
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<ULandscapeGrassType> GrassType;
    TWeakObjectPtr<ULandscapeGrassType> FlowerType;
    FDelegateHandle ActorHandle;
    FDelegateHandle LevelHandle;
    int64 RequestGeneration = 0;
    bool bActive = false;
    bool bRequestAttempted = false;
};
