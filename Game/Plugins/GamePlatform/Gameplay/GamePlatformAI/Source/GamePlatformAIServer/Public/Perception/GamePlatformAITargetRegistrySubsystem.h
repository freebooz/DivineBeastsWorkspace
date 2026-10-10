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
    /** 仅Game/PIE权威世界创建；Editor、预览、Commandlet和纯客户端不运行Actor注册。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    /** 所有真实注册/生成入口都复核世界资格，防止迟到回调在退出或编辑地图时改变Actor。 */
    bool IsAuthorityRuntimeWorld() const;
    FDelegateHandle ActorSpawnedHandle;

    void HandleActorSpawned(AActor* Actor);
    void RegisterActorIfEligible(AActor* Actor);
    void EnsureServerAIController(AActor* Actor);
};
