#pragma once
#include "Engine/BrushBuilder.h"
#include "GamePlatformWaterBrushBuilder.generated.h"

/** 编辑器内部闭合湖岸挤出器；Builder编辑数据不进入服务器，真实Brush碰撞由引擎烘焙。 */
UCLASS()
class UGamePlatformWaterBrushBuilder : public UBrushBuilder
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<FVector2D> Shore;
    UPROPERTY() float DepthCm = 1000.f;
    virtual bool Build(UWorld* World, ABrush* Brush = nullptr) override;
};
