// 仅开发关卡的配置发射夹具：不实现位移、追踪、碰撞或第二套伤害执行器。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "TimerManager.h"
#include "DivineBeastsProjectileMotionDemo.generated.h"
class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatComponent;
class AGamePlatformCombatProjectile;

/** 配置Profile后Standalone/PIE服务器定期发射；空Profile仅作为锁定目标。发行构建不启动。 */
UCLASS(NotBlueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API ADivineBeastsProjectileMotionDemo : public AActor
{
    GENERATED_BODY()
public:
    ADivineBeastsProjectileMotionDemo();
    UPROPERTY(EditAnywhere, Category="Demo") TObjectPtr<UGamePlatformCombatProjectileFlightProfile> FlightProfile;
    UPROPERTY(EditAnywhere, Category="Demo") TObjectPtr<AActor> Target;
    UPROPERTY(EditAnywhere, Category="Demo") FVector LaunchDirection=FVector::ForwardVector;
    /** 通用调试球体独立于Niagara，便于验证没有VFX仍能运动；非正式火球外观。 */
    UPROPERTY(EditAnywhere, Category="Demo") bool bShowDebugMesh=true;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Demo") void LaunchDemo();
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;
    UPROPERTY() TObjectPtr<UGamePlatformCombatComponent> Combat;
    TArray<TWeakObjectPtr<AGamePlatformCombatProjectile>> Projectiles;
    FTimerHandle LaunchTimer;
};
