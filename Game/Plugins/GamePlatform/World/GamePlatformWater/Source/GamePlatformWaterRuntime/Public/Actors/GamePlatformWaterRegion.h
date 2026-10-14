// AGamePlatformWaterRegion（平台通用权威水域区域）：轻量地图Actor，双端使用相同静态数据。
// 它不是可见WaterBody（渲染水体）及不对客户端开放改变深度或流速的RPC。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformWaterTypes.h"
#include "GamePlatformWaterRegion.generated.h"

class UBoxComponent;

/** 水域边界须对应正式地图已烘焙区域，默认无碰撞与无Tick；注册失败时不参与游戏判定。 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLATFORMWATERRUNTIME_API AGamePlatformWaterRegion : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformWaterRegion();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /** 水域Box用于开发者可视化标注范围，但不产生Overlap或者碰撞事件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Water|Authority")
    TObjectPtr<UBoxComponent> WaterBounds;

    /** 含水面高度偏移、浅水阈值与流向，纯共享规则，不含纹理引用。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Water|Authority")
    FGamePlatformWaterRegionConfig Region;

    /** 失效区域不会参与权威查询，报告给日志和审核面板而不是自动修复。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Water")
    bool IsWaterRegionValid() const;

    /** 仅双端内部调用；按Box旋转、边界与配置计算局部水深。 */
    bool SampleRegion(const FVector& WorldPosition, FGamePlatformWaterSample& OutSample) const;

protected:
    UPROPERTY(VisibleDefaultsOnly, Category="Water")
    TObjectPtr<USceneComponent> WaterRoot;
};
