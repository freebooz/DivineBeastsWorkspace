// 平台客户端实体能力：最终骨骼姿态触发真实足底接触，不新增Actor Tick或SFX依赖。
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformSnowContactTypes.h"
#include "GamePlatformSnowFootContactComponent.generated.h"
class USkeletalMeshComponent;

/** 由项目装配到客户端ACharacter；组件与骨骼委托均随实体/World退出释放。 */
UCLASS(ClassGroup=(Surface), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMSURFACECLIENT_API UGamePlatformSnowFootContactComponent final : public UActorComponent
{
    GENERATED_BODY()
public:
    UGamePlatformSnowFootContactComponent();
    /** 注册前指定已有Mesh；不拥有Mesh，不修改动画或角色物理。 */
    void SetContactMesh(USkeletalMeshComponent* InMesh);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void OnBonesFinalized();
    TWeakObjectPtr<USkeletalMeshComponent> ContactMesh;
    FDelegateHandle BonesHandle;
    FGamePlatformSnowFootContactGate Feet[2];
};
