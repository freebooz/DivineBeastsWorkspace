#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlatformAITargetRegistrySubsystem.generated.h"

UCLASS()
class GAMEPLATFORMAISERVER_API UGamePlatformAITargetRegistrySubsystem final
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    FDelegateHandle ActorSpawnedHandle;

    void HandleActorSpawned(AActor* Actor);
    void RegisterActorIfEligible(AActor* Actor);
    void EnsureServerAIController(AActor* Actor);
};
