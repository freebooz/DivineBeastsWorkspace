// 私有原生重入夹具，无资源、移动或战斗职责；仅开发测试设置回调。
#pragma once
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformMotionSource.h"
#include "GamePlatformVFXMotionTestSource.generated.h"
UCLASS(Transient,NotBlueprintable)
class AGamePlatformVFXMotionTestSource : public AActor, public IGamePlatformMotionSource
{
    GENERATED_BODY()
public:
    AGamePlatformVFXMotionTestSource();
    FGamePlatformMotionSnapshot Sample;
    TFunction<void()> OnQuery;
    virtual FGamePlatformMotionSnapshot GetMotionSnapshot_Implementation() const override;
};
