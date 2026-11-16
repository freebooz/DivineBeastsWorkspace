#pragma once
#include "GameFramework/PhysicsVolume.h"
#include "GamePlatformWaterPhysicsVolume.generated.h"

class AGamePlatformWaterRegion;

/** 原生PhysicsVolume的资格适配：只读取唯一水域索引，不计算位移、不复制另一套泳态。
 * 地图静态Actor随World生灭，保留引擎Brush/碰撞、CharacterMovement预测及运动复制。 */
UCLASS()
class GAMEPLATFORMWATERRUNTIME_API AGamePlatformWaterPhysicsVolume : public APhysicsVolume
{
    GENERATED_BODY()
public:
    AGamePlatformWaterPhysicsVolume();

    /** 同级同世界的唯一水域所有者；未绑定、未注册或失效时禁止原生泳态准入。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Water|Authority")
    TObjectPtr<AGamePlatformWaterRegion> WaterRegion;

    /** GT原生体积选择回调；沿用Engine几何，再按共享水域规则过滤，失败不回退授权。 */
    virtual bool IsOverlapInVolume(const USceneComponent& TestComponent) const override;
};
