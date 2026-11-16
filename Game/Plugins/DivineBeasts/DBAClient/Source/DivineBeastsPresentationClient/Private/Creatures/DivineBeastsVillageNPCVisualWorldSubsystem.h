// SSLM新手村NPC的客户端外观装配；只观察复制实体并持有本世界Data租约。
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsVillageNPCVisualWorldSubsystem.generated.h"

class ADivineBeastsVillageNPC;

/** 将Arissa模型和动画应用于三名复制NPC，不触碰服务器权威实体。 */
UCLASS()
class UDivineBeastsVillageNPCVisualWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    TSet<TWeakObjectPtr<ADivineBeastsVillageNPC>> ObservedNPCs;
    FDelegateHandle SpawnHandle;
    FGamePlatformDataLease VisualLease;
    FSoftObjectPath MeshPath;
    FSoftObjectPath AnimPath;
    FGuid Operation;
    bool bLoading = false;
    bool bReady = false;
    bool bClosing = false;

    void ObserveActor(AActor* Spawned);
    void BeginLoadVisuals();
    void ApplyVisual(ADivineBeastsVillageNPC& NPC);
    UFUNCTION() void HandleNPCEndPlay(AActor* Ended, EEndPlayReason::Type Reason);
};
