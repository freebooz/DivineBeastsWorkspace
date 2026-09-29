#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DivineBeastsCharacterAppearanceWorldSubsystem.generated.h"

/**
 * UDivineBeastsCharacterAppearanceWorldSubsystem（神兽联盟客户端角色外观世界子系统）。
 * 为当前客户端世界中的ACharacter挂接轻量外观组件；组件自身只对存在项目角色状态的Actor生效。
 * 子系统无Tick，Actor生成时O(1)处理，世界BeginPlay仅做一次现有角色补扫。
 */
UCLASS()
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsCharacterAppearanceWorldSubsystem final
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    void HandleActorSpawned(AActor* Actor);
    void EnsureAppearanceComponent(AActor* Actor);

    FDelegateHandle ActorSpawnedHandle;
};
