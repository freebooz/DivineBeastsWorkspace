#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlatformArenaServerSubsystem.generated.h"

class FGamePlatformArenaServerBackendClient;
class FGamePlatformArenaServerCoordinator;

/** UGamePlatformArenaServerSubsystem（竞技服务器世界子系统）。
 *  Dedicated Server进入MainArena世界后自动读取安全环境配置并拉取Assignment。
 */
UCLASS()
class GAMEPLATFORMARENASERVER_API UGamePlatformArenaServerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    TSharedPtr<FGamePlatformArenaServerBackendClient> BackendClient;
    TSharedPtr<FGamePlatformArenaServerCoordinator> Coordinator;
};
